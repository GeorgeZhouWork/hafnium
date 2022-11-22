/*
 * Copyright 2018 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#include "hf/cpu.h"

#include <stdalign.h>

#include "hf/arch/cache.h"

#include "hf/api.h"
#include "hf/check.h"
#include "hf/dlog.h"
#include "hf/arch/plat/psci.h"

#include "vmapi/hf/call.h"

#include "system/sys/cdefs.h"

#define STACK_SIZE PAGE_SIZE

/**
 * The stacks to be used by the CPUs.
 *
 * Align to page boundaries to ensure that cache lines are not shared between a
 * CPU's stack and data that can be accessed from other CPUs. If this did
 * happen, there may be coherency problems when the stack is being used before
 * caching is enabled.
 */
alignas(PAGE_SIZE) static char callstacks[MAX_CPUS][STACK_SIZE] __section(
		.stacks);

/* NOLINTNEXTLINE(misc-redundant-expression) */
static_assert((STACK_SIZE % PAGE_SIZE) == 0, "Keep each stack page aligned.");
static_assert((PAGE_SIZE % STACK_ALIGN) == 0,
	      "Page alignment is too weak for the stack.");

/**
 * Internal buffer used to store FF-A messages from a VM Tx. Its usage prevents
 * TOCTOU issues while Hafnium performs actions on information that would
 * otherwise be re-writable by the VM.
 *
 * Each buffer is owned by a single CPU. The buffer can only be used for
 * ffa_msg_send. The information stored in the buffer is only valid during the
 * ffa_msg_send request is performed.
 */
alignas(PAGE_SIZE) static uint8_t cpu_message_buffer[MAX_CPUS][PAGE_SIZE];

static struct spinlock smp_lock;
static int32_t smp_maps[MAX_CPUS] = {0};

uint8_t *cpu_get_buffer(struct cpu *c)
{
	size_t cpu_indx = cpu_index(c);

	CHECK(cpu_indx < MAX_CPUS);

	return cpu_message_buffer[cpu_indx];
}

uint32_t cpu_get_buffer_size(struct cpu *c)
{
	size_t cpu_indx = cpu_index(c);

	CHECK(cpu_indx < MAX_CPUS);

	return sizeof(cpu_message_buffer[cpu_indx]);
}

/* State of all supported CPUs. The stack of the first one is initialized. */
struct cpu cpus[MAX_CPUS] = {
	{
		.is_on = 1,
		.stack_bottom = &callstacks[0][STACK_SIZE],
	},
};

uint32_t cpu_count = 1;

static void smp_maps_init(void)
{
    uint32_t i;

    sl_init(&smp_lock);

    for (i = 0; i < MAX_CPUS; i++) {
        smp_maps[i] = HF_INVALID_VM_ID;
    }
}

struct cpu *cpu_request(ffa_vm_id_t vmid)
{
    int32_t cpu_index = -1;
    uint32_t i;

    sl_lock(&smp_lock);

    for (i = 0; i < cpu_count; i++) {
        if (smp_maps[i] == HF_INVALID_VM_ID) {
            cpu_index = i;
            smp_maps[i] = vmid;
            break;
        }
    }

    sl_unlock(&smp_lock);

    return (cpu_index >= 0) ? &cpus[cpu_index] : NULL;
}

ffa_vm_id_t cpu_get_vmid(struct cpu *cpu)
{
    cpu_id_t cpuid = cpu_index(cpu);
    ffa_vm_id_t vmid;

    sl_lock(&smp_lock);
    vmid = smp_maps[cpuid];
    sl_unlock(&smp_lock);

    return vmid;
}

void cpu_module_init(const cpu_id_t *cpu_ids, size_t count)
{
	uint32_t i;
	uint32_t j;
	cpu_id_t boot_cpu_id = cpus[0].id;
	bool found_boot_cpu = false;

	cpu_count = count;

	/*
	 * Initialize CPUs with the IDs from the configuration passed in. The
	 * CPUs after the boot CPU are initialized in reverse order. The boot
	 * CPU is initialized when it is found or in place of the last CPU if it
	 * is not found.
	 */
    j = 0;
	for (i = 0; i < cpu_count; ++i) {
		struct cpu *c;
		cpu_id_t id = cpu_ids[i];

		if (found_boot_cpu || id != boot_cpu_id) {
			c = &cpus[j];
			c->stack_bottom = &callstacks[j][STACK_SIZE];
		} else {
			found_boot_cpu = true;
			c = &cpus[0];
			CHECK(c->stack_bottom == &callstacks[0][STACK_SIZE]);
		}

        j++;

		sl_init(&c->lock);
		c->id = id;
	}

	if (!found_boot_cpu) {
		/* Boot CPU was initialized but with wrong ID. */
		dlog_warning("Boot CPU's ID not found in config.\n");
		cpus[0].id = boot_cpu_id;
	}

	/*
	 * Clean the cache for the cpus array such that secondary cores
	 * hitting the entry point can read the cpus array consistently
	 * with MMU off (hence data cache off).
	 */
	arch_cache_data_clean_range(va_from_ptr(cpus), sizeof(cpus));

	arch_cache_data_clean_range(va_from_ptr(&cpu_count), sizeof(cpu_count));

    smp_maps_init();
}

size_t cpu_index(struct cpu *c)
{
	return c - cpus;
}

/*
 * Return cpu with the given index.
 */
struct cpu *cpu_find_index(size_t index)
{
	return (index < MAX_CPUS) ? &cpus[index] : NULL;
}

/**
 * Turns CPU on and returns the previous state.
 */
bool cpu_on(struct cpu *c, ipaddr_t entry, uintreg_t arg)
{
	bool prev;

	sl_lock(&c->lock);
	prev = c->is_on;
	c->is_on = true;
	sl_unlock(&c->lock);

	if (!prev) {
		/* This returns the first booted VM (e.g. primary in the NWd) */
		struct vm *vm = vm_get_first_boot();
		struct vcpu *vcpu = vm_get_vcpu(vm, cpu_index(c));
		struct vcpu_locked vcpu_locked;

        if (vcpu == NULL)
            return true;
        
		vcpu_locked = vcpu_lock(vcpu);
		vcpu_on(vcpu_locked, entry, arg);
		vcpu_unlock(&vcpu_locked);
	}

	return prev;
}

/**
 * Prepares the CPU for turning itself off.
 */
void cpu_off(struct cpu *c)
{
	sl_lock(&c->lock);
	c->is_on = false;
	sl_unlock(&c->lock);
}

/**
 * Searches for a CPU based on its ID.
 */
struct cpu *cpu_find(cpu_id_t id)
{
	size_t i;

	for (i = 0; i < cpu_count; i++) {
		if (cpus[i].id == id) {
			return &cpus[i];
		}
	}

	return NULL;
}
