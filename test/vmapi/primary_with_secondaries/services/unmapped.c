/*
 * Copyright 2020 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#include "hf/arch/vm/interrupts.h"

#include "hf/mm.h"
#include "hf/std.h"

#include "vmapi/hf/call.h"

#include "../sysregs.h"
#include "test/hftest.h"
#include "test/vmapi/exception_handler.h"

alignas(PAGE_SIZE) static uint8_t pages[2 * PAGE_SIZE];

TEST_SERVICE(data_unmapped)
{
	/* Not using NULL so static analysis doesn't complain. */
	int *p = (int *)1;
	exception_setup(NULL, exception_handler_yield_data_abort);
	*p = 12;
	FAIL("Exception not generated by invalid access.");
}

TEST_SERVICE(straddling_data_unmapped)
{
	void *send_buf = SERVICE_SEND_BUFFER();
	/* Give some memory to the primary VM so that it's unmapped. */
	struct spci_memory_region_constituent constituents[] = {
		spci_memory_region_constituent_init(
			(uint64_t)(&pages[PAGE_SIZE]), 1),
	};
	uint32_t msg_size = spci_memory_region_init(
		send_buf, hf_vm_get_id(), HF_PRIMARY_VM_ID, constituents,
		ARRAY_SIZE(constituents), 0, 0, SPCI_MEMORY_RW_X,
		SPCI_MEMORY_NORMAL_MEM, SPCI_MEMORY_CACHE_WRITE_BACK,
		SPCI_MEMORY_OUTER_SHAREABLE);
	exception_setup(NULL, exception_handler_yield_data_abort);

	EXPECT_EQ(spci_mem_donate(msg_size, msg_size, 0).func, SPCI_SUCCESS_32);

	*(volatile uint64_t *)(&pages[PAGE_SIZE - 6]);
	FAIL("Exception not generated by invalid access.");
}

TEST_SERVICE(instruction_unmapped)
{
	/* Not using NULL so static analysis doesn't complain. */
	int (*f)(void) = (int (*)(void))4;
	exception_setup(NULL, exception_handler_yield_instruction_abort);
	f();
	FAIL("Exception not generated by invalid access.");
}
