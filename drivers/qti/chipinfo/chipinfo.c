/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <drivers/qti/chipinfo/chipinfo.h>
#include <drivers/qti/platforminfo/platforminfodefs.h>
#include <drivers/qti/smem/smem.h>

#include "chipinfo_internal.h"

static struct chipinfo_ctxt chipinfo_ctxt;

uint32_t chipinfo_get_chip_version(void)
{
	if (!chipinfo_ctxt.initialized) {
		return CHIPINFO_VERSION_UNKNOWN;
	}

	return chipinfo_ctxt.version;
}

enum chipinfo_id chipinfo_get_chip_id(void)
{
	if (!chipinfo_ctxt.initialized) {
		return CHIPINFO_ID_UNKNOWN;
	}

	return chipinfo_ctxt.chipinfo_id;
}

enum chipinfo_family chipinfo_get_chip_family(void)
{
	if (!chipinfo_ctxt.initialized) {
		return CHIPINFO_FAMILY_UNKNOWN;
	}

	return chipinfo_ctxt.family_id;
}

enum chipinfo_result qti_chipinfo_init(void)
{
	struct platforminfo_smem *smem;
	uint32_t size;
	uint32_t fmt;
	uint32_t chip_id;
	uint32_t chip_family;

	/* Access the socinfo SMEM region populated by the boot firmware. */
	smem = (struct platforminfo_smem *)smem_get_addr(SMEM_HW_SW_BUILD_ID,
							 &size);
	if (smem == NULL || size < sizeof(uint32_t)) {
		return CHIPINFO_ERROR_NOT_FOUND;
	}

	/*
	 * Use the SMEM format version to guard field access.
	 * Each version number corresponds to a specific set
	 * of fields that XBL populated; accessing beyond what XBL wrote would
	 * read uninitialised data. The version numbers match go/platforminfo.
	 * chip_id and chip_version are present from version 1 onwards.
	 * chip_family was added in version 12.
	 */
	fmt = smem->format;

	if (fmt < 1U) {
		return CHIPINFO_ERROR_INVALID_DATA;
	}

	/* Bound the raw values before casting into the driver enums. */
	chip_id = smem->chip_id;
	if (chip_id >= CHIPINFO_NUM_IDS) {
		chip_id = CHIPINFO_ID_UNKNOWN;
	}
	chipinfo_ctxt.chipinfo_id = (enum chipinfo_id)chip_id;
	chipinfo_ctxt.version = smem->chip_version;

	if (fmt >= PLATFORMINFO_FORMAT_VER_12) {
		chip_family = smem->chip_family;
		if (chip_family >= CHIPINFO_NUM_FAMILIES) {
			chip_family = CHIPINFO_FAMILY_UNKNOWN;
		}
		chipinfo_ctxt.family_id = (enum chipinfo_family)chip_family;
	}

	chipinfo_ctxt.initialized = true;

	return CHIPINFO_SUCCESS;
}
