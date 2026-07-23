/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * nord_rpmh_mpu_grant_hlos() - grant HLOS (AP-NonSecure / DRV-2) access to the
 * RPMh register space via the RPMH_MPU_XPU4 unmapped-region permission.
 *
 * On Nord the RPMh register space is guarded by RPMH_MPU_XPU4 (base
 * NORD_RPMH_MPU_XPU4_BASE). That MPU has no region groups, so all access is
 * governed by the unmapped-region permission register (UMRPERMREG @ base+0x408):
 *   UMR_QAD[0]  (bit0)  - QAD0 / APPS shared enable
 *   UMR_NS      (bit30) - AP-NonSecure R+W
 *   UMR_S       (bit31) - AP-Secure R+W
 * BL31 runs AP-Secure and can vote RPMh fine; HLOS runs AP-NonSecure. If the
 * boot firmware left UMR_NS clear, non-secure RPMh accesses are blocked while
 * secure ones pass - the exact asymmetry seen in the DRV-2 ACTIVE-TCS timeout.
 * Writing NORD_XPU4_UMR_PERM_HLOS (QAD0 | UMR_NS | UMR_S) grants HLOS R+W while
 * keeping AP-Secure, mirroring the downstream Nord config
 * (AC_DOMAIN_AP_NS_BIT | AC_DOMAIN_APPS_SEC_BIT).
 *
 * This is a targeted grant, not a full XPU v4 driver. It gates on the block
 * actually being a rev>=4.2 MPU (matching the downstream ACXpuSetUnmapped-
 * RegionPerms gate) and logs the before/after UMRPERM value so a boot log tells
 * us whether the boot firmware had tightened AP-NS access in the first place.
 */

#include <stdint.h>

#include <arch_helpers.h>
#include <common/debug.h>
#include <lib/mmio.h>

#include <nord_def.h>
#include <qti_plat.h>

/* IDR0.XPU_TYPE[1:0]: 0 == MPU (AC_MPU). */
#define NORD_XPU4_IDR0_XPU_TYPE_BMSK	0x3U
#define NORD_XPU4_TYPE_MPU		0x0U

/* REV: MAJOR[31:28], MINOR[27:16]; UMRPERMREG exists on rev >= 4.2. */
#define NORD_XPU4_REV_MAJOR_SHFT	28U
#define NORD_XPU4_REV_MINOR_SHFT	16U
#define NORD_XPU4_REV_4_2 \
	((4U << NORD_XPU4_REV_MAJOR_SHFT) | (2U << NORD_XPU4_REV_MINOR_SHFT))

void nord_rpmh_mpu_grant_hlos(void)
{
	uintptr_t base = NORD_RPMH_MPU_XPU4_BASE;
	uint32_t idr0;
	uint32_t rev;
	uint32_t old_perm;
	uint32_t new_perm;

	idr0 = mmio_read_32(base + NORD_XPU4_IDR0_OFFSET);
	if ((idr0 & NORD_XPU4_IDR0_XPU_TYPE_BMSK) != NORD_XPU4_TYPE_MPU) {
		WARN("rpmh-mpu: RPMH_MPU_XPU4 is not an MPU (IDR0=0x%x); skipping\n",
		     idr0);
		return;
	}

	/* UMRPERMREG only exists on rev >= 4.2 (matches the downstream gate). */
	rev = mmio_read_32(base + NORD_XPU4_REV_OFFSET);
	if ((rev & 0xffff0000U) < NORD_XPU4_REV_4_2) {
		WARN("rpmh-mpu: RPMH_MPU_XPU4 rev 0x%x < 4.2; skipping\n", rev);
		return;
	}

	old_perm = mmio_read_32(base + NORD_XPU4_UMRPERMREG_OFFSET);

	mmio_write_32(base + NORD_XPU4_UMRPERMREG_OFFSET,
		      NORD_XPU4_UMR_PERM_HLOS);
	dmbsy();
	isb();

	new_perm = mmio_read_32(base + NORD_XPU4_UMRPERMREG_OFFSET);

	NOTICE("rpmh-mpu: UMRPERM 0x%x -> 0x%x (HLOS AP-NS grant)\n",
	       old_perm, new_perm);

	if (new_perm != NORD_XPU4_UMR_PERM_HLOS) {
		WARN("rpmh-mpu: UMRPERM write-back mismatch (want 0x%x got 0x%x)\n",
		     NORD_XPU4_UMR_PERM_HLOS, new_perm);
	}
}
