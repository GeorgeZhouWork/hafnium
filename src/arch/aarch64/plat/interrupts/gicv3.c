/*
 * Copyright 2021 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#include "hf/types.h"

#include "msr.h"

void plat_interrupts_set_priority_mask(uint8_t min_priority)
{
	write_msr(ICC_PMR_EL1, min_priority);
}
