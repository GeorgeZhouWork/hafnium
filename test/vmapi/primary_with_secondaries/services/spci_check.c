/*
 * Copyright 2020 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#include "hf/arch/irq.h"
#include "hf/arch/vm/interrupts.h"
#include "hf/arch/vm/timer.h"

#include "hf/spci.h"
#include "hf/std.h"

#include "vmapi/hf/call.h"

#include "primary_with_secondary.h"
#include "test/hftest.h"
#include "test/vmapi/spci.h"

TEST_SERVICE(spci_check)
{
	void *recv_buf = SERVICE_RECV_BUFFER();
	const char message[] = "spci_msg_send";

	/* Wait for single message to be sent by the primary VM. */
	struct spci_value ret = spci_msg_wait();

	EXPECT_EQ(ret.func, SPCI_MSG_SEND_32);

	/* Ensure message header has all fields correctly set. */
	EXPECT_EQ(spci_msg_send_size(ret), sizeof(message));
	EXPECT_EQ(spci_msg_send_receiver(ret), hf_vm_get_id());
	EXPECT_EQ(spci_msg_send_sender(ret), HF_PRIMARY_VM_ID);

	/* Ensure that the payload was correctly transmitted. */
	EXPECT_EQ(memcmp(recv_buf, message, sizeof(message)), 0);

	spci_yield();
}

TEST_SERVICE(spci_length)
{
	void *recv_buf = SERVICE_RECV_BUFFER();
	const char message[] = "this should be truncated";

	/* Wait for single message to be sent by the primary VM. */
	struct spci_value ret = spci_msg_wait();

	EXPECT_EQ(ret.func, SPCI_MSG_SEND_32);

	/* Verify the length is as expected. */
	EXPECT_EQ(16, spci_msg_send_size(ret));

	/* Check only part of the message is sent correctly. */
	EXPECT_NE(memcmp(recv_buf, message, sizeof(message)), 0);
	EXPECT_EQ(memcmp(recv_buf, message, spci_msg_send_size(ret)), 0);

	spci_yield();
}

TEST_SERVICE(spci_recv_non_blocking)
{
	/* Wait for single message to be sent by the primary VM. */
	struct spci_value ret = spci_msg_poll();

	EXPECT_SPCI_ERROR(ret, SPCI_RETRY);

	spci_yield();
}

TEST_SERVICE(spci_direct_message_resp_echo)
{
	struct spci_value args = spci_msg_wait();

	EXPECT_EQ(args.func, SPCI_MSG_SEND_DIRECT_REQ_32);

	spci_msg_send_direct_resp(spci_msg_send_receiver(args),
				  spci_msg_send_sender(args), args.arg3,
				  args.arg4, args.arg5, args.arg6, args.arg7);
}

TEST_SERVICE(spci_direct_unallowed_smc)
{
	struct spci_value args = spci_msg_wait();
	struct spci_value ret;

	EXPECT_EQ(args.func, SPCI_MSG_SEND_DIRECT_REQ_32);

	ret = spci_yield();
	EXPECT_SPCI_ERROR(ret, SPCI_DENIED);

	ret = spci_msg_send(spci_msg_send_receiver(args),
			    spci_msg_send_sender(args), 0, 0);
	EXPECT_SPCI_ERROR(ret, SPCI_DENIED);

	ret = spci_msg_wait();
	EXPECT_SPCI_ERROR(ret, SPCI_DENIED);

	ret = spci_msg_poll();
	EXPECT_SPCI_ERROR(ret, SPCI_DENIED);

	ret = spci_msg_send_direct_req(SERVICE_VM1, SERVICE_VM2, 0, 0, 0, 0, 0);
	EXPECT_SPCI_ERROR(ret, SPCI_DENIED);

	spci_msg_send_direct_resp(spci_msg_send_receiver(args),
				  spci_msg_send_sender(args), args.arg3,
				  args.arg4, args.arg5, args.arg6, args.arg7);
}
