/*
 * Copyright 2020 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#pragma once

#include "hf/boot_params.h"
#include "hf/mm.h"
#include "hf/mpool.h"

/** Apply an update to the FDT. */
bool fdt_patch(struct mm_stage1_locked stage1_locked, paddr_t fdt_addr,
	       struct boot_params_update *p, struct mpool *ppool);
