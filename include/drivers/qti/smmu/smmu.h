/********************************************************************
* Copyright (c) 2026 Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: BSD-3-Clause-Clear
 *********************************************************************/
#ifndef SMMU_H
#define SMMU_H


struct smmu_cfg_s {
	uint32_t *addr;
	uint32_t value;
};

/* for platforms that don't support the driver, stub out qti_smmu_init(): */
#ifdef PLAT_QTI_SMMU_DRIVER
void qti_smmu_init(void);
#else
static inline void qti_smmu_init(void) {};
#endif

extern struct smmu_cfg_s qti_smmu_cfg[];
extern const size_t qti_smmu_cfg_count;

#endif