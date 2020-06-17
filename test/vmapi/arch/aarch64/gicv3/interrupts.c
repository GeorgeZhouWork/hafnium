/*
 * Copyright 2018 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#include "hf/arch/irq.h"
#include "hf/arch/vm/interrupts_gicv3.h"

#include "hf/dlog.h"
#include "hf/std.h"

#include "vmapi/hf/call.h"

#include "../msr.h"
#include "gicv3.h"
#include "test/hftest.h"

SET_UP(interrupts)
{
	system_setup();
}

TEST(interrupts, enable_sgi)
{
	/* Interrupt IDs 0 to 15 are SGIs. */
	uint8_t intid = 3;
	interrupt_set_priority_mask(0xff);
	interrupt_set_priority(intid, 0x80);
	arch_irq_enable();
	interrupt_enable_all(true);
	EXPECT_EQ(io_read32_array(GICD_ISPENDR, 0), 0);
	EXPECT_EQ(io_read32(GICR_ISPENDR0), 0);
	EXPECT_EQ(io_read32_array(GICD_ISACTIVER, 0), 0);
	EXPECT_EQ(io_read32(GICR_ISACTIVER0), 0);

	/* Send ourselves the SGI. */
	last_interrupt_id = 0xffffffff;
	dlog("sending SGI\n");
	interrupt_send_sgi(intid, false, 0, 0, 0, 1);
	dlog("sent SGI\n");

	/* Check that we got it, and we are back to not active or pending. */
	EXPECT_EQ(last_interrupt_id, intid);
	EXPECT_EQ(io_read32_array(GICD_ISPENDR, 0), 0);
	EXPECT_EQ(io_read32_array(GICD_ISACTIVER, 0), 0);
	EXPECT_EQ(io_read32(GICR_ISACTIVER0), 0);
}

TEST(interrupts, disable_sgi)
{
	/* Interrupt IDs 0 to 15 are SGIs. */
	uint8_t intid = 3;
	interrupt_enable_all(true);
	interrupt_enable(intid, false);
	interrupt_set_priority_mask(0xff);
	interrupt_set_priority(intid, 0x80);
	arch_irq_enable();
	EXPECT_EQ(io_read32_array(GICD_ISPENDR, 0), 0);
	EXPECT_EQ(io_read32(GICR_ISPENDR0), 0);
	EXPECT_EQ(io_read32_array(GICD_ISACTIVER, 0), 0);
	EXPECT_EQ(io_read32(GICR_ISACTIVER0), 0);

	/* Send ourselves the SGI. */
	last_interrupt_id = 0xffffffff;
	dlog("sending SGI\n");
	interrupt_send_sgi(intid, false, 0, 0, 0, 1);
	dlog("sent SGI\n");

	/* Check that we didn't get it, but it is pending (and not active). */
	EXPECT_EQ(last_interrupt_id, 0xffffffff);
	EXPECT_EQ(io_read32_array(GICD_ISPENDR, 0), 0);
	EXPECT_EQ(io_read32(GICR_ISPENDR0), 0x1 << intid);
	EXPECT_EQ(io_read32_array(GICD_ISACTIVER, 0), 0);
	EXPECT_EQ(io_read32(GICR_ISACTIVER0), 0);
}

TEST(interrupts, physical_timer)
{
	interrupt_enable(PHYSICAL_TIMER_IRQ, true);
	interrupt_set_priority(PHYSICAL_TIMER_IRQ, 0x80);
	interrupt_set_edge_triggered(PHYSICAL_TIMER_IRQ, true);
	interrupt_set_priority_mask(0xff);
	arch_irq_enable();

	/*
	 * Check that no (SGI or PPI) interrupts are active or pending to start
	 * with.
	 */
	EXPECT_EQ(io_read32_array(GICD_ISPENDR, 0), 0);
	EXPECT_EQ(io_read32(GICR_ISPENDR0), 0);
	EXPECT_EQ(io_read32_array(GICD_ISACTIVER, 0), 0);
	EXPECT_EQ(io_read32(GICR_ISACTIVER0), 0);

	dlog("Starting timer\n");
	/* Set physical timer for 1 tick. */
	write_msr(CNTP_TVAL_EL0, 1);
	/* Enable it. */
	write_msr(CNTP_CTL_EL0, 0x00000001);

	dlog("waiting for interrupt\n");
	while (last_interrupt_id == 0) {
		EXPECT_EQ(io_read32_array(GICD_ISPENDR, 0), 0);
		EXPECT_EQ(io_read32(GICR_ISPENDR0), 0);
		EXPECT_EQ(io_read32_array(GICD_ISACTIVER, 0), 0);
		EXPECT_EQ(io_read32(GICR_ISACTIVER0), 0);
	}

	/* Check that we got the interrupt. */
	dlog("Checking for interrupt\n");
	EXPECT_EQ(last_interrupt_id, PHYSICAL_TIMER_IRQ);
	/* Check timer status. */
	EXPECT_EQ(read_msr(CNTP_CTL_EL0), 0x00000005);

	/* There should again be no pending or active interrupts. */
	EXPECT_EQ(io_read32_array(GICD_ISPENDR, 0), 0);
	EXPECT_EQ(io_read32(GICR_ISPENDR0), 0);
	EXPECT_EQ(io_read32_array(GICD_ISACTIVER, 0), 0);
	EXPECT_EQ(io_read32(GICR_ISACTIVER0), 0);
}

TEST(interrupts, virtual_timer)
{
	interrupt_enable(VIRTUAL_TIMER_IRQ, true);
	interrupt_set_priority(VIRTUAL_TIMER_IRQ, 0x80);
	interrupt_set_edge_triggered(VIRTUAL_TIMER_IRQ, true);
	interrupt_set_priority_mask(0xff);
	arch_irq_enable();

	/* Check that no interrupts are active or pending to start with. */
	EXPECT_EQ(io_read32_array(GICD_ISPENDR, 0), 0);
	EXPECT_EQ(io_read32(GICR_ISPENDR0), 0);
	EXPECT_EQ(io_read32_array(GICD_ISACTIVER, 0), 0);
	EXPECT_EQ(io_read32(GICR_ISACTIVER0), 0);

	dlog("Starting timer\n");
	/* Set virtual timer for 1 tick. */
	write_msr(CNTV_TVAL_EL0, 1);
	/* Enable it. */
	write_msr(CNTV_CTL_EL0, 0x00000001);

	dlog("Waiting for interrupt\n");
	while (last_interrupt_id == 0) {
		EXPECT_EQ(io_read32_array(GICD_ISPENDR, 0), 0);
		EXPECT_EQ(io_read32(GICR_ISPENDR0), 0);
		EXPECT_EQ(io_read32_array(GICD_ISACTIVER, 0), 0);
		EXPECT_EQ(io_read32(GICR_ISACTIVER0), 0);
	}

	/* Check that we got the interrupt. */
	dlog("Checking for interrupt\n");
	EXPECT_EQ(last_interrupt_id, VIRTUAL_TIMER_IRQ);
	/* Check timer status. */
	EXPECT_EQ(read_msr(CNTV_CTL_EL0), 0x00000005);

	/* There should again be no pending or active interrupts. */
	EXPECT_EQ(io_read32_array(GICD_ISPENDR, 0), 0);
	EXPECT_EQ(io_read32(GICR_ISPENDR0), 0);
	EXPECT_EQ(io_read32_array(GICD_ISACTIVER, 0), 0);
	EXPECT_EQ(io_read32(GICR_ISACTIVER0), 0);
}
