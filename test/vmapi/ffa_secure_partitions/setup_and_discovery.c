/*
 * Copyright 2021 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#include "hf/dlog.h"
#include "hf/ffa.h"

#include "vmapi/hf/call.h"

#include "partition_services.h"
#include "test/hftest.h"
#include "test/vmapi/ffa.h"

TEAR_DOWN(ffa)
{
	EXPECT_FFA_ERROR(ffa_rx_release(), FFA_DENIED);
}

static void check_v1_1_partition_info_descriptors(
	const struct ffa_partition_info *partitions)
{
	struct ffa_uuid uuid;

	/* Expect the PVM as first partition. */
	EXPECT_EQ(partitions[0].vm_id, hf_vm_get_id());
	EXPECT_EQ(partitions[0].vcpu_count, 8);
	ffa_uuid_init(0xb4b5671e, 0x4a904fe1, 0xb81ffb13, 0xdae1dacb, &uuid);
	EXPECT_TRUE(ffa_uuid_equal(&partitions[0].uuid, &uuid));

	/* Expect a SP as second partition. */
	EXPECT_EQ(partitions[1].vm_id, SP_ID(1));
	EXPECT_TRUE(partitions[1].vcpu_count == 8 ||
		    partitions[1].vcpu_count == 1);
	ffa_uuid_init(0xa609f132, 0x6b4f, 0x4c14, 0x9489, &uuid);
	EXPECT_TRUE(ffa_uuid_equal(&partitions[1].uuid, &uuid));

	/* Expect secondary SP as third partition */
	EXPECT_EQ(partitions[2].vm_id, SP_ID(2));
	EXPECT_EQ(partitions[2].vcpu_count, 8);
	ffa_uuid_init(0x9458bb2d, 0x353b4ee2, 0xaa25710c, 0x99b73ddc, &uuid);
	EXPECT_TRUE(ffa_uuid_equal(&partitions[2].uuid, &uuid));
}

TEST(ffa, ffa_partition_info_get_uuid_null)
{
	struct mailbox_buffers mb;
	struct ffa_value ret;
	const struct ffa_partition_info *partitions;
	struct ffa_uuid uuid;

	/* Setup the mailbox (which holds the RX buffer). */
	mb = set_up_mailbox();
	partitions = mb.recv;

	/*
	 * A Null UUID requests information for all partitions
	 * including VMs and SPs.
	 */
	ffa_uuid_init(0, 0, 0, 0, &uuid);

	/* Check that expected partition information is returned. */
	ret = ffa_partition_info_get(&uuid, 0);
	EXPECT_EQ(ret.func, FFA_SUCCESS_32);

	/* Expect three partitions. */
	EXPECT_EQ(ret.arg2, 3);

	/* Check the partition info descritor size returned in w3 is correct. */
	EXPECT_EQ(ret.arg3, sizeof(struct ffa_partition_info));

	/* Expect the PVM as first partition. */
	check_v1_1_partition_info_descriptors(partitions);

	EXPECT_EQ(ffa_rx_release().func, FFA_SUCCESS_32);
}

TEST(ffa, ffa_partition_info_get_count_flag)
{
	struct ffa_value ret;
	struct ffa_uuid uuid;

	ffa_uuid_init(0, 0, 0, 0, &uuid);

	ret = ffa_partition_info_get(&uuid, FFA_PARTITION_COUNT_FLAG);
	EXPECT_EQ(ret.func, FFA_SUCCESS_32);

	/* Expect three partitions. */
	EXPECT_EQ(ret.arg2, 3);

	/* Check when the count flag is set w3 MBZ. */
	EXPECT_EQ(ret.arg3, 0);
}

TEST(ffa, ffa_partition_info_get_flags_mbz_fail)
{
	struct ffa_value ret;
	struct ffa_uuid uuid;

	ffa_uuid_init(0, 0, 0, 0, &uuid);

	ret = ffa_partition_info_get(&uuid, 0xffff);
	EXPECT_FFA_ERROR(ret, FFA_INVALID_PARAMETERS);
}

TEST(ffa, ffa_partition_info_get_uuid_fixed)
{
	struct mailbox_buffers mb;
	struct ffa_value ret;
	const struct ffa_partition_info *partitions;
	struct ffa_uuid uuid;

	/* Setup the mailbox (which holds the RX buffer). */
	mb = set_up_mailbox();
	partitions = mb.recv;

	/* Search for a known secure partition UUID. */
	ffa_uuid_init(0xa609f132, 0x6b4f, 0x4c14, 0x9489, &uuid);

	/* Check that a partition count of 1 is returned. */
	ret = ffa_partition_info_get(&uuid, FFA_PARTITION_COUNT_FLAG);
	EXPECT_EQ(ret.func, FFA_SUCCESS_32);

	/* Expect one partition. */
	EXPECT_EQ(ret.arg2, 1);

	/* Check when the count flag is set w3 MBZ. */
	EXPECT_EQ(ret.arg3, 0);

	/* And that the buffer is zero */
	EXPECT_EQ(partitions[0].vm_id, 0);
	EXPECT_EQ(partitions[0].vcpu_count, 0);

	/* Check that the expected partition information is returned. */
	ret = ffa_partition_info_get(&uuid, 0);
	EXPECT_EQ(ret.func, FFA_SUCCESS_32);

	/* Expect one partition. */
	EXPECT_EQ(ret.arg2, 1);

	/* Check the partition info descritor size returned in w3 is correct. */
	EXPECT_EQ(ret.arg3, sizeof(struct ffa_partition_info));

	/* Expect a secure partition. */
	EXPECT_EQ(partitions[0].vm_id, HF_SPMC_VM_ID + 1);
	EXPECT_TRUE(partitions[0].vcpu_count == 8 ||
		    partitions[0].vcpu_count == 1);

	/*
	 * If a uuid is specified (not null) ensure the uuid returned in the
	 * partition info descriptor is zeroed.
	 */
	EXPECT_TRUE(ffa_uuid_is_null(&partitions[0].uuid));

	EXPECT_EQ(ffa_rx_release().func, FFA_SUCCESS_32);
}

TEST(ffa, ffa_partition_info_get_uuid_unknown)
{
	struct ffa_value ret;
	struct ffa_uuid uuid;

	/* Search for a unknown partition UUID. */
	ffa_uuid_init(1, 1, 1, 1, &uuid);

	/* Expect no partition is found with such UUID. */
	ret = ffa_partition_info_get(&uuid, 0);
	EXPECT_EQ(ret.func, FFA_ERROR_32);
}

TEST(ffa, ffa_partition_info_get_versioned_descriptors)
{
	struct mailbox_buffers mb;
	struct ffa_value ret;
	const struct ffa_partition_info_v1_0 *partitions_v1_0;
	const struct ffa_partition_info *partitions_v1_1;
	struct ffa_uuid uuid;
	uint32_t version;

	/* Set version 1.1 to forward RX/TX buffers to SPMC. */
	ffa_version(MAKE_FFA_VERSION(1, 1));

	/* Setup the mailbox (which holds the RX buffer). */
	mb = set_up_mailbox();

	/*
	 * First call FF-A version to tell the SPMC our version
	 * is v1.0.
	 */
	version = ffa_version(MAKE_FFA_VERSION(1, 0));
	EXPECT_EQ(version, FFA_VERSION_COMPILED);

	partitions_v1_0 = mb.recv;

	/*
	 * A Null UUID requests information for all partitions
	 * including VMs and SPs.
	 */
	ffa_uuid_init(0, 0, 0, 0, &uuid);

	/* Check that expected partition information is returned. */
	ret = ffa_partition_info_get(&uuid, 0);
	EXPECT_EQ(ret.func, FFA_SUCCESS_32);

	/* Expect three partitions. */
	EXPECT_EQ(ret.arg2, 3);

	/* Check the partition info descritor size returned in w3 is correct. */
	EXPECT_EQ(ret.arg3, sizeof(struct ffa_partition_info_v1_0));

	/* Expect the PVM as first partition. */
	EXPECT_EQ(partitions_v1_0[0].vm_id, hf_vm_get_id());
	EXPECT_TRUE(partitions_v1_0[1].vcpu_count == 8 ||
		    partitions_v1_0[1].vcpu_count == 1);

	/* Expect a SP as second partition. */
	EXPECT_EQ(partitions_v1_0[1].vm_id, SP_ID(1));
	EXPECT_EQ(partitions_v1_0[0].vcpu_count, 8);

	/* Expect secondary SP as third partition. */
	EXPECT_EQ(partitions_v1_0[2].vm_id, SP_ID(2));
	EXPECT_EQ(partitions_v1_0[2].vcpu_count, 8);

	EXPECT_EQ(ffa_rx_release().func, FFA_SUCCESS_32);

	/* Set version to v1.1 to see if uuid is now returned. */
	version = ffa_version(MAKE_FFA_VERSION(1, 1));
	EXPECT_EQ(version, FFA_VERSION_COMPILED);

	partitions_v1_1 = mb.recv;

	/* Check that expected partition information is returned. */
	ret = ffa_partition_info_get(&uuid, 0);
	EXPECT_EQ(ret.func, FFA_SUCCESS_32);

	/* Expect three partitions. */
	EXPECT_EQ(ret.arg2, 3);

	/* Check the partition info descritor size returned in w3 is correct. */
	EXPECT_EQ(ret.arg3, sizeof(struct ffa_partition_info));

	check_v1_1_partition_info_descriptors(partitions_v1_1);

	EXPECT_EQ(ffa_rx_release().func, FFA_SUCCESS_32);
}

/*
 * Check FFA_SPM_ID_GET response.
 * DEN0077A FF-A v1.1 Beta0 section 13.9 FFA_SPM_ID_GET.
 */
TEST(ffa, ffa_spm_id_get)
{
	struct ffa_value ret = ffa_spm_id_get();

	EXPECT_EQ(ret.func, FFA_SUCCESS_32);

	/* Expect the SPMC FF-A ID at NS virtual FF-A instance. */
	EXPECT_EQ(ret.arg2, HF_SPMC_VM_ID);
}
