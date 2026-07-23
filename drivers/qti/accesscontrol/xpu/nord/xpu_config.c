/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Nord (SA8797P) compiled-in XPU v4 static access-control policy.
 *
 * Transcribed from the XBL-generated Nord access-control dataset
 * (ACConfigDataLib/nord/build/{ACTargetData.c,ACXpuStaticConfig.c}). This is
 * the "Option B" compiled policy - the equivalent of the downstream
 * g_acGlobalDataLegacyConfig / tzACCfgEntryLegacy fallback, and of TF-A's
 * existing xpu3 kodiak static config.
 *
 * SAFE, INCREMENTAL SUBSET: this first cut only programs instances whose
 * downstream policy is plain-static (address windows + fixed QAD permission
 * vectors, or UMR-only). XPRESSCFG / device-programmed instances (whose
 * permissions are read back from HW, not stored) are intentionally omitted -
 * add them only after the static path is validated on target.
 *
 * QAD permission vector bit layout (written verbatim to RGRDRn/RGWRRn and
 * UMRPERMREG): bit0=QAD0/APPS, bit30=AP-NonSecure, bit31=AP-Secure,
 * bits1-10=other QADs.
 */

#include <stdint.h>

#include <lib/utils_def.h>

#include <nord_def.h>
#include <xpu4.h>

/* QAD domain bits (Nord dataset ACXpuStaticConfig.c). */
#define QAD_APPS_BIT		BIT(0)
#define AC_DOMAIN_AP_NS		(BIT(0) | BIT(30))	/* AP-NonSecure R/W */
#define AC_DOMAIN_APPS_SEC	(BIT(0) | BIT(31))	/* AP-Secure R/W    */

/*
 * RPMH_MPU_XPU4 (HAL_XPU2_AOSS_RPMH_MPU) - guards the RPMh register space.
 * nrg == 0: access is governed entirely by the unmapped-region permission.
 * Grant AP-NonSecure (HLOS/DRV-2) + AP-Secure R/W, matching the downstream
 * .umrPerm = AC_DOMAIN_AP_NS_BIT | AC_DOMAIN_APPS_SEC_BIT (== 0xC0000001).
 * (nord_rpmh_mpu_grant_hlos() also writes this same value with a before/after
 * NOTICE; both writes are idempotent.)
 */
static const struct xpu4_instance nord_xpu_cfg[] = {
	{
		.base     = NORD_RPMH_MPU_XPU4_BASE,	/* 0x0C292000 */
		.xpu_id   = 0U,				/* AOSS_RPMH_MPU */
		.rgs      = NULL,
		.nrg      = 0U,
		.umr_perm = AC_DOMAIN_AP_NS | AC_DOMAIN_APPS_SEC,
		.cfg_owner = 0U,			/* QAD_APPS */
		.flags    = XPU4_INST_SET_UMR | XPU4_INST_ERR_REPORT,
	},
};

const struct xpu4_instance *nord_xpu_get_config(uint32_t *count)
{
	*count = (uint32_t)ARRAY_SIZE(nord_xpu_cfg);
	return nord_xpu_cfg;
}
