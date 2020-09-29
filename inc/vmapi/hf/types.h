/*
 * Copyright 2020 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#pragma once

/* Define the standard types for the platform. */
#if defined(__linux__) && defined(__KERNEL__)

#include <linux/types.h>

#define INT32_C(c) c

typedef phys_addr_t hf_ipaddr_t;

#else

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uintptr_t hf_ipaddr_t;

#endif

/**
 * An offset to use when assigning VM IDs.
 * The offset is needed because VM ID 0 is reserved.
 */
#define HF_VM_ID_OFFSET 1

/**
 * The index and ID of the primary VM, which is responsible for scheduling.
 *
 * These are not equal because ID 0 is reserved for the hypervisor itself.
 * Primary VM therefore gets ID 1 and all other VMs come after that.
 */
#define HF_PRIMARY_VM_INDEX 0
#if SECURE_WORLD == 0
#define HF_PRIMARY_VM_ID (HF_VM_ID_OFFSET + HF_PRIMARY_VM_INDEX)
#else
#define HF_PRIMARY_VM_ID ((HF_VM_ID_OFFSET + HF_PRIMARY_VM_INDEX) | (SPMC_SECURE_ID_MASK << SPMC_SECURE_ID_SHIFT))
#endif

/**
 * The special VM ID reserved for the OS running in the trusted execution
 * environment, e.g. secure EL1 on AArch64.
 */
#define HF_TEE_VM_ID 0x8000

/** Sleep value for an indefinite period of time. */
#define HF_SLEEP_INDEFINITE 0xffffffffffffffff

/** The amount of data that can be sent to a mailbox. */
#define HF_MAILBOX_SIZE 4096

/** The number of virtual interrupt IDs which are supported. */
#define HF_NUM_INTIDS 64

/** Interrupt ID returned when there is no interrupt pending. */
#define HF_INVALID_INTID 0xffffffff

/** Interrupt ID indicating the mailbox is readable. */
#define HF_MAILBOX_READABLE_INTID 1

/** Interrupt ID indicating a mailbox is writable. */
#define HF_MAILBOX_WRITABLE_INTID 2

/** The virtual interrupt ID used for the virtual timer. */
#define HF_VIRTUAL_TIMER_INTID 3
