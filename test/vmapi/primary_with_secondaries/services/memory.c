/*
 * Copyright 2018 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#include "hf/arch/vm/interrupts.h"

#include "hf/check.h"
#include "hf/mm.h"
#include "hf/std.h"

#include "vmapi/hf/call.h"

#include "primary_with_secondary.h"
#include "test/hftest.h"
#include "test/vmapi/exception_handler.h"
#include "test/vmapi/ffa.h"

alignas(PAGE_SIZE) static uint8_t page[PAGE_SIZE];
static uint8_t retrieve_buffer[PAGE_SIZE * 2];

TEST_SERVICE(memory_increment)
{
	/* Loop, writing message to the shared memory. */
	for (;;) {
		size_t i;
		void *recv_buf = SERVICE_RECV_BUFFER();
		void *send_buf = SERVICE_SEND_BUFFER();

		struct ffa_value ret = ffa_msg_wait();
		struct ffa_memory_region *memory_region =
			(struct ffa_memory_region *)retrieve_buffer;
		ffa_vm_id_t sender = retrieve_memory_from_message(
			recv_buf, send_buf, ret, NULL, memory_region,
			HF_MAILBOX_SIZE);
		struct ffa_composite_memory_region *composite =
			ffa_memory_region_get_composite(memory_region, 0);
		uint8_t *ptr = (uint8_t *)composite->constituents[0].address;

		ASSERT_EQ(memory_region->receiver_count, 1);
		ASSERT_NE(memory_region->receivers[0]
				  .composite_memory_region_offset,
			  0);

		/* Allow the memory to be populated. */
		EXPECT_EQ(ffa_yield().func, FFA_SUCCESS_32);

		/* Increment each byte of memory. */
		for (i = 0; i < PAGE_SIZE; ++i) {
			++ptr[i];
		}

		/* Signal completion and reset. */
		ffa_msg_send(hf_vm_get_id(), sender, sizeof(ptr), 0);
	}
}

TEST_SERVICE(give_memory_and_fault)
{
	void *send_buf = SERVICE_SEND_BUFFER();
	struct ffa_memory_region_constituent constituents[] = {
		{.address = (uint64_t)&page, .page_count = 1},
	};

	/* Give memory to the primary. */
	send_memory_and_retrieve_request(
		FFA_MEM_DONATE_32, send_buf, hf_vm_get_id(), HF_PRIMARY_VM_ID,
		constituents, ARRAY_SIZE(constituents),
		FFA_MEMORY_REGION_FLAG_CLEAR, FFA_DATA_ACCESS_NOT_SPECIFIED,
		FFA_DATA_ACCESS_RW, FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED,
		FFA_INSTRUCTION_ACCESS_X);

	exception_setup(NULL, exception_handler_yield_data_abort);

	/* Try using the memory that isn't valid unless it's been returned. */
	page[16] = 123;

	FAIL("Exception not generated by invalid access.");
}

TEST_SERVICE(lend_memory_and_fault)
{
	void *send_buf = SERVICE_SEND_BUFFER();
	struct ffa_memory_region_constituent constituents[] = {
		{.address = (uint64_t)&page, .page_count = 1},
	};

	/* Lend memory to the primary. */
	send_memory_and_retrieve_request(
		FFA_MEM_LEND_32, send_buf, hf_vm_get_id(), HF_PRIMARY_VM_ID,
		constituents, ARRAY_SIZE(constituents),
		FFA_MEMORY_REGION_FLAG_CLEAR, FFA_DATA_ACCESS_RW,
		FFA_DATA_ACCESS_RW, FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED,
		FFA_INSTRUCTION_ACCESS_X);

	exception_setup(NULL, exception_handler_yield_data_abort);

	/* Try using the memory that isn't valid unless it's been returned. */
	page[633] = 180;

	FAIL("Exception not generated by invalid access.");
}

TEST_SERVICE(ffa_memory_return)
{
	struct ffa_value ret = ffa_msg_wait();
	uint8_t *ptr;
	size_t i;
	void *recv_buf = SERVICE_RECV_BUFFER();
	void *send_buf = SERVICE_SEND_BUFFER();

	exception_setup(NULL, exception_handler_yield_data_abort);

	struct ffa_memory_region *memory_region =
		(struct ffa_memory_region *)retrieve_buffer;
	ffa_vm_id_t sender = retrieve_memory_from_message(
		recv_buf, send_buf, ret, NULL, memory_region, HF_MAILBOX_SIZE);
	struct ffa_composite_memory_region *composite =
		ffa_memory_region_get_composite(memory_region, 0);

	ptr = (uint8_t *)composite->constituents[0].address;

	/* Check that one has access to the shared region. */
	for (i = 0; i < PAGE_SIZE; ++i) {
		ptr[i]++;
	}

	/* Give the memory back and notify the sender. */
	send_memory_and_retrieve_request(
		FFA_MEM_DONATE_32, send_buf, hf_vm_get_id(), sender,
		composite->constituents, composite->constituent_count, 0,
		FFA_DATA_ACCESS_NOT_SPECIFIED, FFA_DATA_ACCESS_RW,
		FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED, FFA_INSTRUCTION_ACCESS_X);

	/*
	 * Try and access the memory which will cause a fault unless the memory
	 * has been shared back again.
	 */
	ptr[0] = 123;

	FAIL("Exception not generated by invalid access.");
}

/**
 * Attempt to modify above the upper bound of a memory region sent to us.
 */
TEST_SERVICE(ffa_check_upper_bound)
{
	struct ffa_memory_region *memory_region;
	struct ffa_composite_memory_region *composite;
	uint8_t *ptr;
	uint8_t index;

	void *recv_buf = SERVICE_RECV_BUFFER();
	void *send_buf = SERVICE_SEND_BUFFER();
	struct ffa_value ret = ffa_msg_wait();

	exception_setup(NULL, exception_handler_yield_data_abort);

	memory_region = (struct ffa_memory_region *)retrieve_buffer;
	retrieve_memory_from_message(recv_buf, send_buf, ret, NULL,
				     memory_region, HF_MAILBOX_SIZE);
	composite = ffa_memory_region_get_composite(memory_region, 0);

	/* Choose which constituent we want to test. */
	index = *(uint8_t *)composite->constituents[0].address;
	ptr = (uint8_t *)composite->constituents[index].address;

	/*
	 * Check that we can't access out of bounds after the region sent to us.
	 * This should trigger the exception handler.
	 */
	ptr[PAGE_SIZE]++;

	FAIL("Exception not generated by access out of bounds.");
}

/**
 * Attempt to modify below the lower bound of a memory region sent to us.
 */
TEST_SERVICE(ffa_check_lower_bound)
{
	struct ffa_memory_region *memory_region;
	struct ffa_composite_memory_region *composite;
	uint8_t *ptr;
	uint8_t index;

	void *recv_buf = SERVICE_RECV_BUFFER();
	void *send_buf = SERVICE_SEND_BUFFER();
	struct ffa_value ret = ffa_msg_wait();

	exception_setup(NULL, exception_handler_yield_data_abort);

	memory_region = (struct ffa_memory_region *)retrieve_buffer;
	retrieve_memory_from_message(recv_buf, send_buf, ret, NULL,
				     memory_region, HF_MAILBOX_SIZE);
	composite = ffa_memory_region_get_composite(memory_region, 0);

	/* Choose which constituent we want to test. */
	index = *(uint8_t *)composite->constituents[0].address;
	ptr = (uint8_t *)composite->constituents[index].address;

	/*
	 * Check that we can't access out of bounds before the region sent to
	 * us. This should trigger the exception handler.
	 */
	ptr[-1]++;

	FAIL("Exception not generated by access out of bounds.");
}

/**
 * Attempt to donate memory and then modify.
 */
TEST_SERVICE(ffa_donate_secondary_and_fault)
{
	uint8_t *ptr;
	void *recv_buf = SERVICE_RECV_BUFFER();
	void *send_buf = SERVICE_SEND_BUFFER();

	struct ffa_value ret = ffa_msg_wait();
	struct ffa_memory_region *memory_region =
		(struct ffa_memory_region *)retrieve_buffer;
	ffa_vm_id_t sender = retrieve_memory_from_message(
		recv_buf, send_buf, ret, NULL, memory_region, HF_MAILBOX_SIZE);
	struct ffa_composite_memory_region *composite =
		ffa_memory_region_get_composite(memory_region, 0);

	ASSERT_EQ(sender, HF_PRIMARY_VM_ID);
	exception_setup(NULL, exception_handler_yield_data_abort);

	ptr = (uint8_t *)composite->constituents[0].address;

	/* Donate memory to next VM. */
	send_memory_and_retrieve_request(
		FFA_MEM_DONATE_32, send_buf, hf_vm_get_id(), SERVICE_VM2,
		composite->constituents, composite->constituent_count, 0,
		FFA_DATA_ACCESS_NOT_SPECIFIED, FFA_DATA_ACCESS_RW,
		FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED, FFA_INSTRUCTION_ACCESS_X);

	/* Ensure that we are unable to modify memory any more. */
	ptr[0] = 'c';

	FAIL("Exception not generated by invalid access.");
}

/**
 * Attempt to donate memory twice from VM.
 */
TEST_SERVICE(ffa_donate_twice)
{
	uint32_t msg_size;
	void *recv_buf = SERVICE_RECV_BUFFER();
	void *send_buf = SERVICE_SEND_BUFFER();

	struct ffa_value ret = ffa_msg_wait();
	struct ffa_memory_region *memory_region =
		(struct ffa_memory_region *)retrieve_buffer;
	ffa_vm_id_t sender = retrieve_memory_from_message(
		recv_buf, send_buf, ret, NULL, memory_region, HF_MAILBOX_SIZE);
	struct ffa_composite_memory_region *composite =
		ffa_memory_region_get_composite(memory_region, 0);
	struct ffa_memory_region_constituent constituent =
		composite->constituents[0];

	/* Yield to allow attempt to re donate from primary. */
	ffa_yield();

	/* Give the memory back and notify the sender. */
	send_memory_and_retrieve_request(
		FFA_MEM_DONATE_32, send_buf, hf_vm_get_id(), sender,
		&constituent, 1, 0, FFA_DATA_ACCESS_NOT_SPECIFIED,
		FFA_DATA_ACCESS_RW, FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED,
		FFA_INSTRUCTION_ACCESS_X);

	/* Attempt to donate the memory to another VM. */
	EXPECT_EQ(
		ffa_memory_region_init(
			send_buf, HF_MAILBOX_SIZE, hf_vm_get_id(), SERVICE_VM2,
			&constituent, 1, 0, 0, FFA_DATA_ACCESS_NOT_SPECIFIED,
			FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED,
			FFA_MEMORY_NORMAL_MEM, FFA_MEMORY_CACHE_WRITE_BACK,
			FFA_MEMORY_OUTER_SHAREABLE, NULL, &msg_size),
		0);
	EXPECT_FFA_ERROR(ffa_mem_donate(msg_size, msg_size), FFA_DENIED);

	ffa_yield();
}

/**
 * Continually receive memory, check if we have access and ensure it is not
 * changed by a third party.
 */
TEST_SERVICE(ffa_memory_receive)
{
	void *recv_buf = SERVICE_RECV_BUFFER();
	void *send_buf = SERVICE_SEND_BUFFER();

	for (;;) {
		struct ffa_value ret = ffa_msg_wait();
		struct ffa_memory_region *memory_region =
			(struct ffa_memory_region *)retrieve_buffer;
		struct ffa_composite_memory_region *composite;
		uint8_t *ptr;

		retrieve_memory_from_message(recv_buf, send_buf, ret, NULL,
					     memory_region, HF_MAILBOX_SIZE);
		composite = ffa_memory_region_get_composite(memory_region, 0);
		ptr = (uint8_t *)composite->constituents[0].address;

		ptr[0] = 'd';
		ffa_yield();

		/* Ensure memory has not changed. */
		EXPECT_EQ(ptr[0], 'd');
		ffa_yield();
	}
}

/**
 * Receive memory and attempt to donate from primary VM.
 */
TEST_SERVICE(ffa_donate_invalid_source)
{
	uint32_t msg_size;
	void *recv_buf = SERVICE_RECV_BUFFER();
	void *send_buf = SERVICE_SEND_BUFFER();

	struct ffa_value ret = ffa_msg_wait();
	struct ffa_memory_region *memory_region =
		(struct ffa_memory_region *)retrieve_buffer;
	ffa_vm_id_t sender = retrieve_memory_from_message(
		recv_buf, send_buf, ret, NULL, memory_region, HF_MAILBOX_SIZE);
	struct ffa_composite_memory_region *composite =
		ffa_memory_region_get_composite(memory_region, 0);

	/* Give the memory back and notify the sender. */
	send_memory_and_retrieve_request(
		FFA_MEM_DONATE_32, send_buf, hf_vm_get_id(), sender,
		composite->constituents, composite->constituent_count, 0,
		FFA_DATA_ACCESS_NOT_SPECIFIED, FFA_DATA_ACCESS_RW,
		FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED, FFA_INSTRUCTION_ACCESS_X);

	/* Fail to donate the memory from the primary to VM2. */
	EXPECT_EQ(ffa_memory_region_init(
			  send_buf, HF_MAILBOX_SIZE, HF_PRIMARY_VM_ID,
			  SERVICE_VM2, composite->constituents,
			  composite->constituent_count, 0, 0,
			  FFA_DATA_ACCESS_NOT_SPECIFIED,
			  FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED,
			  FFA_MEMORY_NORMAL_MEM, FFA_MEMORY_CACHE_WRITE_BACK,
			  FFA_MEMORY_OUTER_SHAREABLE, NULL, &msg_size),
		  0);
	EXPECT_FFA_ERROR(ffa_mem_donate(msg_size, msg_size),
			 FFA_INVALID_PARAMETERS);
	ffa_yield();
}

TEST_SERVICE(ffa_memory_lend_relinquish)
{
	exception_setup(NULL, exception_handler_yield_data_abort);

	/* Loop, giving memory back to the sender. */
	for (;;) {
		size_t i;
		ffa_memory_handle_t handle;

		void *recv_buf = SERVICE_RECV_BUFFER();
		void *send_buf = SERVICE_SEND_BUFFER();

		struct ffa_value ret = ffa_msg_wait();
		struct ffa_memory_region *memory_region =
			(struct ffa_memory_region *)retrieve_buffer;
		ffa_vm_id_t sender = retrieve_memory_from_message(
			recv_buf, send_buf, ret, &handle, memory_region,
			sizeof(retrieve_buffer));
		struct ffa_composite_memory_region *composite =
			ffa_memory_region_get_composite(memory_region, 0);
		struct ffa_memory_region_constituent *constituents;
		uint8_t *first_ptr;

		/* ASSERT_TRUE isn't enough for clang-analyze. */
		CHECK(composite != NULL);
		constituents = composite->constituents;
		first_ptr = (uint8_t *)constituents[0].address;

		/*
		 * Check that we can read and write every page that was shared.
		 */
		for (i = 0; i < composite->constituent_count; ++i) {
			uint8_t *ptr = (uint8_t *)constituents[i].address;
			uint32_t count = constituents[i].page_count;
			size_t j;

			for (j = 0; j < PAGE_SIZE * count; ++j) {
				ptr[j]++;
			}
		}

		/* Give the memory back and notify the sender. */
		ffa_mem_relinquish_init(send_buf, handle, 0, hf_vm_get_id());
		EXPECT_EQ(ffa_mem_relinquish().func, FFA_SUCCESS_32);
		EXPECT_EQ(ffa_msg_send(hf_vm_get_id(), sender, 0, 0).func,
			  FFA_SUCCESS_32);

		/*
		 * Try to access the memory, which will cause a fault unless the
		 * memory has been shared back again.
		 */
		first_ptr[0] = 123;
	}
}

/**
 * Ensure that we can't relinquish donated memory.
 */
TEST_SERVICE(ffa_memory_donate_relinquish)
{
	for (;;) {
		size_t i;
		ffa_memory_handle_t handle;
		struct ffa_memory_region *memory_region;
		struct ffa_composite_memory_region *composite;
		uint8_t *ptr;

		void *recv_buf = SERVICE_RECV_BUFFER();
		void *send_buf = SERVICE_SEND_BUFFER();
		struct ffa_value ret = ffa_msg_wait();

		memory_region = (struct ffa_memory_region *)retrieve_buffer;
		retrieve_memory_from_message(recv_buf, send_buf, ret, &handle,
					     memory_region, HF_MAILBOX_SIZE);
		composite = ffa_memory_region_get_composite(memory_region, 0);

		ptr = (uint8_t *)composite->constituents[0].address;

		/* Check that we have access to the shared region. */
		for (i = 0; i < PAGE_SIZE; ++i) {
			ptr[i]++;
		}

		/*
		 * Attempt to relinquish the memory, which should fail because
		 * it was donated not lent.
		 */
		ffa_mem_relinquish_init(send_buf, handle, 0, hf_vm_get_id());
		EXPECT_FFA_ERROR(ffa_mem_relinquish(), FFA_INVALID_PARAMETERS);

		/* Ensure we still have access to the memory. */
		ptr[0] = 123;

		ffa_yield();
	}
}

/**
 * Receive memory that has been shared, try to relinquish it with the clear flag
 * set (and expect to fail), and then relinquish without any flags.
 */
TEST_SERVICE(ffa_memory_share_relinquish_clear)
{
	exception_setup(NULL, exception_handler_yield_data_abort);

	/* Loop, receiving memory and relinquishing it. */
	for (;;) {
		ffa_memory_handle_t handle;

		void *recv_buf = SERVICE_RECV_BUFFER();
		void *send_buf = SERVICE_SEND_BUFFER();

		struct ffa_value ret = ffa_msg_wait();
		ffa_vm_id_t sender = retrieve_memory_from_message(
			recv_buf, send_buf, ret, &handle, NULL,
			HF_MAILBOX_SIZE);

		/* Trying to relinquish the memory and clear it should fail. */
		ffa_mem_relinquish_init(send_buf, handle,
					FFA_MEMORY_REGION_FLAG_CLEAR,
					hf_vm_get_id());
		EXPECT_FFA_ERROR(ffa_mem_relinquish(), FFA_INVALID_PARAMETERS);

		/* Give the memory back and notify the sender. */
		ffa_mem_relinquish_init(send_buf, handle, 0, hf_vm_get_id());
		EXPECT_EQ(ffa_mem_relinquish().func, FFA_SUCCESS_32);
		EXPECT_EQ(ffa_msg_send(hf_vm_get_id(), sender, 0, 0).func,
			  FFA_SUCCESS_32);
	}
}

/**
 * Receive memory and attempt to donate from primary VM.
 */
TEST_SERVICE(ffa_lend_invalid_source)
{
	ffa_memory_handle_t handle;
	uint32_t msg_size;

	void *recv_buf = SERVICE_RECV_BUFFER();
	void *send_buf = SERVICE_SEND_BUFFER();
	struct ffa_value ret = ffa_msg_wait();
	struct ffa_memory_region *memory_region =
		(struct ffa_memory_region *)retrieve_buffer;
	ffa_vm_id_t sender =
		retrieve_memory_from_message(recv_buf, send_buf, ret, &handle,
					     memory_region, HF_MAILBOX_SIZE);
	struct ffa_composite_memory_region *composite =
		ffa_memory_region_get_composite(memory_region, 0);

	/* Give the memory back and notify the sender. */
	ffa_mem_relinquish_init(send_buf, handle, 0, hf_vm_get_id());
	EXPECT_EQ(ffa_mem_relinquish().func, FFA_SUCCESS_32);
	EXPECT_EQ(ffa_msg_send(hf_vm_get_id(), sender, 0, 0).func,
		  FFA_SUCCESS_32);

	/* Ensure we cannot lend from the primary to another secondary. */
	EXPECT_EQ(ffa_memory_region_init(
			  send_buf, HF_MAILBOX_SIZE, HF_PRIMARY_VM_ID,
			  SERVICE_VM2, composite->constituents,
			  composite->constituent_count, 0, 0,
			  FFA_DATA_ACCESS_RW, FFA_INSTRUCTION_ACCESS_X,
			  FFA_MEMORY_NORMAL_MEM, FFA_MEMORY_CACHE_WRITE_BACK,
			  FFA_MEMORY_OUTER_SHAREABLE, NULL, &msg_size),
		  0);
	EXPECT_FFA_ERROR(ffa_mem_lend(msg_size, msg_size),
			 FFA_INVALID_PARAMETERS);

	/* Ensure we cannot share from the primary to another secondary. */
	EXPECT_EQ(ffa_memory_region_init(
			  send_buf, HF_MAILBOX_SIZE, HF_PRIMARY_VM_ID,
			  SERVICE_VM2, composite->constituents,
			  composite->constituent_count, 0, 0,
			  FFA_DATA_ACCESS_RW, FFA_INSTRUCTION_ACCESS_X,
			  FFA_MEMORY_NORMAL_MEM, FFA_MEMORY_CACHE_WRITE_BACK,
			  FFA_MEMORY_OUTER_SHAREABLE, NULL, &msg_size),
		  0);
	EXPECT_FFA_ERROR(ffa_mem_share(msg_size, msg_size),
			 FFA_INVALID_PARAMETERS);

	ffa_yield();
}

/**
 * Attempt to execute an instruction from the lent memory.
 */
TEST_SERVICE(ffa_memory_lend_relinquish_X)
{
	exception_setup(NULL, exception_handler_yield_instruction_abort);

	for (;;) {
		ffa_memory_handle_t handle;
		void *recv_buf = SERVICE_RECV_BUFFER();
		void *send_buf = SERVICE_SEND_BUFFER();
		struct ffa_value ret = ffa_msg_wait();
		struct ffa_memory_region *memory_region =
			(struct ffa_memory_region *)retrieve_buffer;
		ffa_vm_id_t sender = retrieve_memory_from_message(
			recv_buf, send_buf, ret, &handle, memory_region,
			HF_MAILBOX_SIZE);
		struct ffa_composite_memory_region *composite =
			ffa_memory_region_get_composite(memory_region, 0);
		struct ffa_memory_region_constituent *constituents;
		uint64_t *ptr;

		/* ASSERT_TRUE isn't enough for clang-analyze. */
		CHECK(composite != NULL);

		constituents = composite->constituents;
		ptr = (uint64_t *)constituents[0].address;

		/*
		 * Verify that the instruction in memory is the encoded RET
		 * instruction.
		 */
		EXPECT_EQ(*ptr, 0xD65F03C0);
		/* Try to execute instruction from the shared memory region. */
		__asm__ volatile("blr %0" ::"r"(ptr));

		/* Release the memory again. */
		ffa_mem_relinquish_init(send_buf, handle, 0, hf_vm_get_id());
		EXPECT_EQ(ffa_mem_relinquish().func, FFA_SUCCESS_32);
		EXPECT_EQ(ffa_msg_send(hf_vm_get_id(), sender, 0, 0).func,
			  FFA_SUCCESS_32);
	}
}

/**
 * Attempt to retrieve a shared page but expect to fail.
 */
TEST_SERVICE(ffa_memory_share_fail)
{
	for (;;) {
		void *recv_buf = SERVICE_RECV_BUFFER();
		void *send_buf = SERVICE_SEND_BUFFER();
		struct ffa_value ret = ffa_msg_wait();
		ffa_vm_id_t sender = retrieve_memory_from_message_expect_fail(
			recv_buf, send_buf, ret, FFA_DENIED);

		/* Return control to primary. */
		EXPECT_EQ(ffa_msg_send(hf_vm_get_id(), sender, 0, 0).func,
			  FFA_SUCCESS_32);
	}
}

/**
 * Attempt to read and write to a shared page.
 */
TEST_SERVICE(ffa_memory_lend_relinquish_RW)
{
	exception_setup(NULL, exception_handler_yield_data_abort);

	for (;;) {
		ffa_memory_handle_t handle;
		uint8_t *ptr;
		size_t i;

		void *recv_buf = SERVICE_RECV_BUFFER();
		void *send_buf = SERVICE_SEND_BUFFER();
		struct ffa_value ret = ffa_msg_wait();
		struct ffa_memory_region *memory_region =
			(struct ffa_memory_region *)retrieve_buffer;
		ffa_vm_id_t sender = retrieve_memory_from_message(
			recv_buf, send_buf, ret, &handle, memory_region,
			HF_MAILBOX_SIZE);
		struct ffa_composite_memory_region *composite =
			ffa_memory_region_get_composite(memory_region, 0);
		struct ffa_memory_region_constituent constituent_copy =
			composite->constituents[0];

		ptr = (uint8_t *)constituent_copy.address;

		/* Check that we have read access. */
		for (i = 0; i < PAGE_SIZE; ++i) {
			EXPECT_EQ(ptr[i], 'b');
		}

		/* Return control to primary, to verify shared access. */
		ffa_yield();

		/* Attempt to modify the memory. */
		for (i = 0; i < PAGE_SIZE; ++i) {
			ptr[i]++;
		}

		/* Give the memory back and notify the sender. */
		ffa_mem_relinquish_init(send_buf, handle, 0, hf_vm_get_id());
		EXPECT_EQ(ffa_mem_relinquish().func, FFA_SUCCESS_32);
		EXPECT_EQ(ffa_msg_send(hf_vm_get_id(), sender, 0, 0).func,
			  FFA_SUCCESS_32);
	}
}

TEST_SERVICE(ffa_memory_lend_twice)
{
	struct ffa_value ret = ffa_msg_wait();
	uint8_t *ptr;
	uint32_t msg_size;
	size_t i;

	void *recv_buf = SERVICE_RECV_BUFFER();
	void *send_buf = SERVICE_SEND_BUFFER();
	struct ffa_memory_region *memory_region;
	struct ffa_composite_memory_region *composite;
	struct ffa_memory_region_constituent constituent_copy;

	memory_region = (struct ffa_memory_region *)retrieve_buffer;
	retrieve_memory_from_message(recv_buf, send_buf, ret, NULL,
				     memory_region, HF_MAILBOX_SIZE);
	composite = ffa_memory_region_get_composite(memory_region, 0);
	constituent_copy = composite->constituents[0];

	ptr = (uint8_t *)constituent_copy.address;

	/* Check that we have read access. */
	for (i = 0; i < PAGE_SIZE; ++i) {
		EXPECT_EQ(ptr[i], 'b');
	}

	/* Attempt to modify the memory. */
	for (i = 0; i < PAGE_SIZE; ++i) {
		ptr[i]++;
	}

	for (i = 1; i < PAGE_SIZE * 2; i++) {
		constituent_copy.address = (uint64_t)ptr + i;

		/* Fail to lend or share the memory from the primary. */
		EXPECT_EQ(ffa_memory_region_init(
				  send_buf, HF_MAILBOX_SIZE, HF_PRIMARY_VM_ID,
				  SERVICE_VM2, &constituent_copy, 1, 0, 0,
				  FFA_DATA_ACCESS_RW, FFA_INSTRUCTION_ACCESS_X,
				  FFA_MEMORY_NORMAL_MEM,
				  FFA_MEMORY_CACHE_WRITE_BACK,
				  FFA_MEMORY_OUTER_SHAREABLE, NULL, &msg_size),
			  0);
		EXPECT_FFA_ERROR(ffa_mem_lend(msg_size, msg_size),
				 FFA_INVALID_PARAMETERS);
		EXPECT_EQ(ffa_memory_region_init(
				  send_buf, HF_MAILBOX_SIZE, HF_PRIMARY_VM_ID,
				  SERVICE_VM2, &constituent_copy, 1, 0, 0,
				  FFA_DATA_ACCESS_RW, FFA_INSTRUCTION_ACCESS_X,
				  FFA_MEMORY_NORMAL_MEM,
				  FFA_MEMORY_CACHE_WRITE_BACK,
				  FFA_MEMORY_OUTER_SHAREABLE, NULL, &msg_size),
			  0);
		EXPECT_FFA_ERROR(ffa_mem_share(msg_size, msg_size),
				 FFA_INVALID_PARAMETERS);
	}

	/* Return control to primary. */
	ffa_yield();
}
