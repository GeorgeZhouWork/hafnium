/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdbool.h>

#include "hf/mm.h"

#define MAX_MEM_RANGES 20

struct mem_range {
	paddr_t begin;
	paddr_t end;
};

struct boot_params {
	struct mem_range mem_ranges[MAX_MEM_RANGES];
	size_t mem_ranges_count;
	paddr_t initrd_begin;
	paddr_t initrd_end;
	size_t kernel_arg;
};

struct boot_params_update {
	struct mem_range reserved_ranges[MAX_MEM_RANGES];
	size_t reserved_ranges_count;
	paddr_t initrd_begin;
	paddr_t initrd_end;
};

bool plat_get_boot_params(struct boot_params *p);
bool plat_update_boot_params(struct boot_params_update *p);
