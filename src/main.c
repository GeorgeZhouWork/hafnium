/*
 * Copyright 2020 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#include "hf/cpu.h"
#include "hf/vm.h"

/**
 * The entry point of CPUs when they are turned on. It is supposed to initialise
 * all state and return the first vCPU to run.
 */
struct vcpu *cpu_main(struct cpu *c)
{
	struct vcpu *vcpu;

	vcpu = vm_get_vcpu(vm_find(HF_PRIMARY_VM_ID), cpu_index(c));
	vcpu->cpu = c;

	arch_cpu_init();

	/* Reset the registers to give a clean start for the primary's vCPU. */
	arch_regs_reset(vcpu);

	return vcpu;
}
