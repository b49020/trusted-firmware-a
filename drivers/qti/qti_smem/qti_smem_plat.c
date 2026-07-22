/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Qualcomm Shared Memory (SMEM) - TFA platform integration
 *
 */
#include <errno.h>

#include <common/debug.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>

#include "qti_smem.h"
#include "qti_smem_plat.h"

/**
 * qti_smem_plat_map() - Map a SMEM offset range into the VA window.
 */
int qti_smem_plat_map(uint32_t offset, size_t size, uint32_t flags)
{
	(void)flags;

	/* Validate that offset + size does not exceed the SMEM region. */
	if ((uint64_t)offset + (uint64_t)size > (uint64_t)QTI_SMEM_SIZE)
		return -EINVAL;

	return 0;
}

/**
 * qti_smem_plat_get_addr() - Translate a SMEM offset to a virtual address.
 */
void *qti_smem_plat_get_addr(uint32_t offset)
{
	/*
	 * SMEM is statically mapped as part of QTI_DEVICE region.
	 * Since it's 1:1 mapping, use physical address directly as virtual address.
	 */
	if ((size_t)offset >= QTI_SMEM_SIZE)
		return NULL;

	return (void *)(QTI_SMEM_BASE + (uint64_t)offset);
}

/**
 * qti_smem_plat_init() - Platform entry point for SMEM initialization.
 *
 * Return: 0 on success, negative errno on failure.
 */
int qti_smem_plat_init(struct qti_smem_plat_info *plat_info)
{
	int ret;

	if (plat_info == NULL)
		return -EINVAL;

	ret = qti_smem_host_id(QTI_SMEM_PROC_TZ, 0, 0, 0,
				&plat_info->local_host);

	if (ret != 0)
		return ret;

	plat_info->max_items = 0xFFFF;
	plat_info->smem_size = QTI_SMEM_SIZE;

	return 0;
}
