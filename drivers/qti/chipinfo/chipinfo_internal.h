/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Private state for the ChipInfo driver. Not part of the public API; consumers
 * must use the accessors declared in <drivers/qti/chipinfo/chipinfo.h>.
 */

#ifndef CHIPINFO_INTERNAL_H
#define CHIPINFO_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include <drivers/qti/chipinfo/chipinfo.h>
#include <drivers/qti/platforminfo/platforminfodefs.h>

struct chipinfo_ctxt {
	bool initialized;
	enum chipinfo_id chipinfo_id;
	uint32_t version;
	enum chipinfo_family family_id;
	/* Indexed by enum chipinfo_part; non-zero means disabled on this SKU. */
	uint32_t disabled_features[CHIPINFO_NUM_PARTS];
	/* Per-instance table pointing into SMEM; NULL if format < 23. */
	const struct platforminfo_part_info *part_info;
	uint32_t num_part_info;
};

#endif /* CHIPINFO_INTERNAL_H */
