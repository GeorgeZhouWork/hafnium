/*
 * Copyright 2018 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

extern "C" {

#include "hf/fdt.h"
}

#include <gmock/gmock.h>

namespace
{
using ::testing::Eq;

/*
 * /dts-v1/;
 *
 * / {
 *       model = "SomeModel";
 *       compatible = "Nothing";
 *       #address-cells = <2>;
 *       #size-cells = <2>;
 *
 *       memory@0 {
 *           device_type = "memory";
 *           reg = <0x00000000 0x00000000 0x00000000 0x20000000>;
 *       };
 *
 *       cpus {
 *           #address-cells = <1>;
 *           #size-cells = <0>;
 *       };
 *
 * };
 *
 * $ dtc --boot-cpu 0 --in-format dts --out-format dtb --out-version 17 test.dts
 * | xxd -i
 */

const uint8_t test_dtb[] = {
	0xd0, 0x0d, 0xfe, 0xed, 0x00, 0x00, 0x01, 0x44, 0x00, 0x00, 0x00, 0x38,
	0x00, 0x00, 0x01, 0x08, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x11,
	0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c,
	0x00, 0x00, 0x00, 0xd0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0a,
	0x00, 0x00, 0x00, 0x00, 0x53, 0x6f, 0x6d, 0x65, 0x4d, 0x6f, 0x64, 0x65,
	0x6c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08,
	0x00, 0x00, 0x00, 0x06, 0x4e, 0x6f, 0x74, 0x68, 0x69, 0x6e, 0x67, 0x00,
	0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x11,
	0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
	0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
	0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x40, 0x30, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x2c,
	0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
	0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x63, 0x70, 0x75, 0x73,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
	0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03,
	0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x09,
	0x6d, 0x6f, 0x64, 0x65, 0x6c, 0x00, 0x63, 0x6f, 0x6d, 0x70, 0x61, 0x74,
	0x69, 0x62, 0x6c, 0x65, 0x00, 0x23, 0x61, 0x64, 0x64, 0x72, 0x65, 0x73,
	0x73, 0x2d, 0x63, 0x65, 0x6c, 0x6c, 0x73, 0x00, 0x23, 0x73, 0x69, 0x7a,
	0x65, 0x2d, 0x63, 0x65, 0x6c, 0x6c, 0x73, 0x00, 0x64, 0x65, 0x76, 0x69,
	0x63, 0x65, 0x5f, 0x74, 0x79, 0x70, 0x65, 0x00, 0x72, 0x65, 0x67, 0x00};

TEST(fdt, total_size)
{
	size_t size;

	EXPECT_TRUE(fdt_size_from_header(&test_dtb[0], &size));
	EXPECT_THAT(size, Eq(sizeof(test_dtb)));
}

} /* namespace */
