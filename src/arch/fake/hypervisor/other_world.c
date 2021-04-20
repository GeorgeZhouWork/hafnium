/*
 * Copyright 2020 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#include "hf/arch/other_world.h"

#include "hf/dlog.h"
#include "hf/ffa.h"
#include "hf/ffa_internal.h"

void arch_other_world_log_init(void)
{
	dlog_info("Initializing Hafnium\n");
}

struct ffa_value arch_other_world_call(struct ffa_value args)
{
	dlog_error("Attempted to call TEE function %#x\n", args.func);
	return ffa_error(FFA_NOT_SUPPORTED);
}

bool arch_other_world_is_direct_request_valid(struct vcpu *current,
					      ffa_vm_id_t sender_vm_id,
					      ffa_vm_id_t receiver_vm_id)
{
	(void)current;
	(void)sender_vm_id;
	(void)receiver_vm_id;

	return true;
}

bool arch_other_world_is_direct_response_valid(struct vcpu *current,
					       ffa_vm_id_t sender_vm_id,
					       ffa_vm_id_t receiver_vm_id)
{
	(void)current;
	(void)sender_vm_id;
	(void)receiver_vm_id;

	return true;
}

bool arch_other_world_direct_request_forward(ffa_vm_id_t receiver_vm_id,
					     struct ffa_value args,
					     struct ffa_value *ret)
{
	(void)receiver_vm_id;
	(void)args;
	(void)ret;
	return false;
}
