/********************************************************************
* Copyright (c) 2026 Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: BSD-3-Clause-Clear
 *********************************************************************/
#include <stdint.h>
#include <stddef.h>
#include "drivers/qti/smmu/smmu.h"
#include <lib/utils_def.h>
#include <stdio.h>


#ifdef PLAT_QTI_SMMU_DRIVER
void qti_smmu_init(void)
{
	for (int i = 0; i < qti_smmu_cfg_count; i++) {
		*((volatile uint32_t *)qti_smmu_cfg[i].addr) = qti_smmu_cfg[i].value;
	}
}
#endif