/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * XPU v4 (MPU) static access-control programming for TF-A.
 *
 * Ported from the downstream Qualcomm XPU v4 HAL (HALxPU4.c). This is the
 * TF-A-native, static "lock down assets" path: it programs a compiled-in set
 * of XPU4 MPU instances (region-group address windows + per-QAD read/write
 * permission vectors, and the unmapped-region permission) from a platform
 * config table. It does NOT implement the dynamic VM mem-assign SMC path or
 * the violation ISR.
 *
 * Permission vectors (read_qads / write_qads / umr_perm) are written verbatim
 * to the RGRDRn / RGWRRn / UMRPERMREG registers, whose bit layout is:
 *   bit0      QAD0 / APPS (shared)
 *   bits1-10  per-QAD (TME_ROM, TME_FW, DEBUG, SRM, MSA, SP, SM, OOBM_*)
 *   bit30     AP-NonSecure (RDA_NS / WRA_NS / UMR_NS)
 *   bit31     AP-Secure    (RDA_S  / WRA_S  / UMR_S)
 */

#ifndef XPU4_H
#define XPU4_H

#include <stdint.h>

/* xpu4_rg flags */
#define XPU4_RG_ENABLE		0x1U	/* set RGCR1n.RGE */
#define XPU4_RG_WOWP		0x2U	/* set RGCR0n.RGWOWP (write-once) */

/* xpu4_instance flags */
#define XPU4_INST_SET_UMR	0x1U	/* program UMRPERMREG from umr_perm */
#define XPU4_INST_SET_CFGOWNER	0x2U	/* program CFGOWNER from cfg_owner */
#define XPU4_INST_ERR_REPORT	0x4U	/* enable client error reporting (CLERE) */

/*
 * struct xpu4_rg - one region group in an MPU instance.
 * @rg_num:     hardware region-group index.
 * @start,@end: physical address window (SoC/CPU view; XPU-local == SoC on Nord).
 * @read_qads:  RGRDRn value (QAD read-permission vector).
 * @write_qads: RGWRRn value (QAD write-permission vector).
 * @flags:      XPU4_RG_*.
 */
struct xpu4_rg {
	uint32_t rg_num;
	uint64_t start;
	uint64_t end;
	uint32_t read_qads;
	uint32_t write_qads;
	uint32_t flags;
};

/*
 * struct xpu4_instance - one XPU4 MPU hardware instance.
 * @base:      MMIO base address of the XPU4 block.
 * @xpu_id:    identifier (for logging only).
 * @rgs:       array of region groups to program (may be NULL if nrg == 0).
 * @nrg:       number of region groups in @rgs.
 * @umr_perm:  UMRPERMREG value (used when XPU4_INST_SET_UMR set).
 * @cfg_owner: CFGOWNER value (used when XPU4_INST_SET_CFGOWNER set).
 * @flags:     XPU4_INST_*.
 */
struct xpu4_instance {
	uintptr_t base;
	uint32_t xpu_id;
	const struct xpu4_rg *rgs;
	uint32_t nrg;
	uint32_t umr_perm;
	uint32_t cfg_owner;
	uint32_t flags;
};

/*
 * Program a compiled-in set of XPU4 MPU instances. Non-MPU or absent blocks
 * are skipped. Safe to call once during BL31 setup.
 */
void xpu4_apply_static_config(const struct xpu4_instance *insts,
			      uint32_t count);

/*
 * Register the XPU violation summary ISR (INTID 0xE3). @insts/@count are
 * retained so the ISR can log/clear the error status of config'd instances.
 * Returns 0 on success. Call after xpu4_apply_static_config().
 */
int xpu4_register_isr(const struct xpu4_instance *insts, uint32_t count);

/*
 * Platform-provided compiled-in XPU4 policy (see nord/xpu_config.c).
 * Returns the instance table and sets *count.
 */
const struct xpu4_instance *nord_xpu_get_config(uint32_t *count);

#endif /* XPU4_H */
