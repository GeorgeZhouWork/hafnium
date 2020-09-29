/*
 * Copyright 2020 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#include "hf/io.h"
#include "hf/mm.h"
#include "hf/mpool.h"
#include "hf/plat/console.h"

/* UART Data Register. */
#define UARTDR IO32_C(PL011_BASE + 0x0)

/* UART Flag Register. */
#define UARTFR IO32_C(PL011_BASE + 0x018)

/* UART Flag Register bit: transmit fifo is full. */
#define UARTFR_TXFF (1 << 5)

/* UART Flag Register bit: UART is busy. */
#define UARTFR_BUSY (1 << 3)

void plat_console_init(void)
{
	/* No hardware initialisation required. */
}

void plat_console_mm_init(struct mm_stage1_locked stage1_locked,
			  struct mpool *ppool)
{
	/* Map page for UART. */
	mm_identity_map(stage1_locked, pa_init(PL011_BASE),
			pa_add(pa_init(PL011_BASE), PAGE_SIZE),
			MM_MODE_R | MM_MODE_W | MM_MODE_D, ppool);
}

void plat_console_putchar(char c)
{
	/* Print a carriage-return as well. */
	if (c == '\n') {
		plat_console_putchar('\r');
	}

	/* Wait until there is room in the tx buffer. */
	while (io_read32(UARTFR) & UARTFR_TXFF) {
		/* do nothing */
	}

	/* Write the character out, force memory access ordering. */
	memory_ordering_barrier();
	io_write32(UARTDR, c);
	memory_ordering_barrier();

	/* Wait until the UART is no longer busy. */
	while (io_read32_mb(UARTFR) & UARTFR_BUSY) {
		/* do nothing */
	}
}
