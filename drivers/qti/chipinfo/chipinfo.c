/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stddef.h>
#include <stdint.h>

#include <drivers/qti/chipinfo/chipinfo.h>
#include <drivers/qti/smem/smem.h>

static struct chipinfo_ctxt chipinfo_ctxt;

uint32_t chipinfo_get_chip_version(void)
{
	return chipinfo_ctxt.version;
}

enum chipinfo_id chipinfo_get_chip_id(void)
{
	return chipinfo_ctxt.chipinfo_id;
}

enum chipinfo_family chipinfo_get_chip_family(void)
{
	return chipinfo_ctxt.family_id;
}

enum chipinfo_result qti_chipinfo_init(void)
{
	struct platforminfo_smem *smem;
	uint32_t size;

	/* Access the socinfo SMEM region populated by the boot firmware. */
	smem = (struct platforminfo_smem *)smem_get_addr(SMEM_HW_SW_BUILD_ID,
							 &size);
	if (smem == NULL || size == 0) {
		return CHIPINFO_ERROR_NOT_FOUND;
	}

	chipinfo_ctxt.chipinfo_id = (enum chipinfo_id)smem->chip_id;
	chipinfo_ctxt.version = smem->chip_version;
	chipinfo_ctxt.family_id = (enum chipinfo_family)smem->chip_family;

	return CHIPINFO_SUCCESS;
}
