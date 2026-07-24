/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Nord DRAM-based XPU v4 access-control policy discovery ("Option A").
 *
 * XBL authenticates a separately-signed AC-config image
 * (SECBOOT_TZ_AC_CONFIG_SW_TYPE) and, per the downstream nord-tz reference
 * (AccessControlTzCfgParser.c), hands its physical address to consumers at
 * runtime rather than at a fixed carveout. TF-A has no hook into that
 * image-authentication framework, so this driver expects the address to
 * have been separately published to SMEM_NORD_AC_CONFIG_ADDR (see
 * nord_ac_config_smem.h - no XBL-side publisher exists yet; this is the
 * TF-A side of that contract, built ahead of it).
 *
 * Blob layout (verified against nord-tz ACTargetConfig.h / HALxPU4.h):
 *
 *   ACGlobalData_t (24B, 64-bit link):
 *     u32 uMagicCookie; u32 uVersion; u32 uNumEntries; <4B pad>;
 *     uintptr_t *pGlobaldataPtr;
 *
 *   pGlobaldataPtr[] is an array of uNumEntries raw absolute PAs, indexed by
 *   AC_XPU_TARGET_CFG_t. Every value - including nested pointers reachable
 *   from it - is baked in at the AC-config image's build/link time; there is
 *   no base-relative fixup. pGlobaldataPtr[AC_XPUCFG_ARRAY] is the ACXpuCfg[]
 *   base PA directly; pGlobaldataPtr[AC_XPUCFG_ARRAY_SIZE] is the PA of a
 *   uint32_t holding that array's element count.
 *
 *   ACXpuCfg (40B, align 8): u32 baseAddr; u32 profileFlags; u32 xpuId;
 *     u16 status; u16 nrg; <union ptr> rg; u32 cfgOwner; u32 umrPerm;
 *     <ptr> rgIgnored. .rg points to a separate array of .nrg region-group
 *     entries (ACMpuRG, 32B: u64 start; u64 end; u32 profileFlags;
 *     u32 readQads; u32 writeQads; u16 rgNum).
 *
 * Every dereference below is preceded by a bounds check and a dynamic,
 * read-only MMU mapping (qti_mmap_add_dynamic_region()), which also refuses
 * to map over secure ranges - any structural surprise aborts the parse and
 * returns NULL rather than guessing, so callers fall back to a compiled
 * policy instead of risking a wild read against XPU-protected DRAM.
 */

#include <errno.h>
#include <stdint.h>

#include <common/debug.h>
#include <drivers/qti/qti_smem/qti_smem.h>
#include <lib/xlat_tables/xlat_tables_v2.h>

#include <drivers/qti/accesscontrol/nord_ac_config_smem.h>
#include <qti_plat.h>
#include <xpu4.h>

#include "ac_config_parser.h"

/* AC_TZ_MAGIC_COOKIE (ACTargetConfig.h). */
#define AC_TZ_MAGIC_COOKIE		0x072382d3U

/* AC_XPU_TARGET_CFG_t indices used (ACTargetConfig.h). */
#define AC_XPUCFG_ARRAY_IDX		3U
#define AC_XPUCFG_ARRAY_SIZE_IDX	4U

/* Bound on uNumEntries: AC_XPU_CFG_MAX (ACTargetConfig.h enum sentinel). */
#define AC_CONFIG_MAX_ENTRIES		70U

/* Bounds on parsed data - not present in the downstream format, chosen here
 * to cap the cost of a malformed/adversarial blob.
 */
#define AC_CONFIG_MAX_INSTANCES		64U
#define AC_CONFIG_MAX_RG_PER_INST	16U

/* ACGlobalData_t layout offsets. */
#define AC_GLOBALDATA_HDR_SIZE		24U
#define AC_GLOBALDATA_OFF_MAGIC		0U
#define AC_GLOBALDATA_OFF_NUMENTRIES	8U
#define AC_GLOBALDATA_OFF_PTR		16U

/* ACXpuCfg layout offsets/size. */
#define ACXPUCFG_ENTRY_SIZE		40U
#define ACXPUCFG_OFF_BASEADDR		0U
#define ACXPUCFG_OFF_XPUID		8U
#define ACXPUCFG_OFF_STATUS		12U
#define ACXPUCFG_OFF_NRG		14U
#define ACXPUCFG_OFF_RG			16U
#define ACXPUCFG_OFF_CFGOWNER		24U
#define ACXPUCFG_OFF_UMRPERM		28U
#define XPU_STATUS_DISABLED		0U

/* ACMpuRG layout offsets/size. */
#define ACMPU_RG_ENTRY_SIZE		32U
#define ACMPU_RG_OFF_START		0U
#define ACMPU_RG_OFF_END		8U
#define ACMPU_RG_OFF_READQADS		20U
#define ACMPU_RG_OFF_WRITEQADS		24U
#define ACMPU_RG_OFF_RGNUM		28U

static struct xpu4_instance nord_ac_parsed_insts[AC_CONFIG_MAX_INSTANCES];
static struct xpu4_rg
	nord_ac_parsed_rgs[AC_CONFIG_MAX_INSTANCES][AC_CONFIG_MAX_RG_PER_INST];

static int read_u32_at_pa(uint64_t pa, uint32_t *out)
{
	int ret = qti_mmap_add_dynamic_region((uintptr_t)pa, sizeof(uint32_t),
					      MT_RO_DATA | MT_SECURE);
	if (ret != 0) {
		return ret;
	}

	*out = *(const uint32_t *)(uintptr_t)pa;
	qti_mmap_remove_dynamic_region((uintptr_t)pa, sizeof(uint32_t));

	return 0;
}

static int read_global_header(uint64_t hdr_pa, uint32_t *num_entries_out,
			      uint64_t *globaldata_ptr_pa_out)
{
	int ret;
	uint32_t magic;

	ret = qti_mmap_add_dynamic_region((uintptr_t)hdr_pa,
					  AC_GLOBALDATA_HDR_SIZE,
					  MT_RO_DATA | MT_SECURE);
	if (ret != 0) {
		VERBOSE("ac_config: header map failed (%d)\n", ret);
		return ret;
	}

	magic = *(const uint32_t *)(uintptr_t)(hdr_pa + AC_GLOBALDATA_OFF_MAGIC);
	if (magic != AC_TZ_MAGIC_COOKIE) {
		VERBOSE("ac_config: magic mismatch 0x%x\n", magic);
		qti_mmap_remove_dynamic_region((uintptr_t)hdr_pa,
					       AC_GLOBALDATA_HDR_SIZE);
		return -EINVAL;
	}

	*num_entries_out = *(const uint32_t *)
		(uintptr_t)(hdr_pa + AC_GLOBALDATA_OFF_NUMENTRIES);
	*globaldata_ptr_pa_out = *(const uint64_t *)
		(uintptr_t)(hdr_pa + AC_GLOBALDATA_OFF_PTR);

	qti_mmap_remove_dynamic_region((uintptr_t)hdr_pa, AC_GLOBALDATA_HDR_SIZE);

	return 0;
}

/* Read pGlobaldataPtr[AC_XPUCFG_ARRAY] and [AC_XPUCFG_ARRAY_SIZE]. Caller has
 * already bounded num_entries so both indices are in range.
 */
static int read_xpucfg_locators(uint64_t arr_pa, uint32_t num_entries,
				uint64_t *xpucfg_arr_pa_out,
				uint64_t *xpucfg_count_pa_out)
{
	int ret;
	size_t map_size = (size_t)num_entries * sizeof(uint64_t);
	const uint64_t *arr;

	ret = qti_mmap_add_dynamic_region((uintptr_t)arr_pa, map_size,
					  MT_RO_DATA | MT_SECURE);
	if (ret != 0) {
		VERBOSE("ac_config: globaldata array map failed (%d)\n", ret);
		return ret;
	}

	arr = (const uint64_t *)(uintptr_t)arr_pa;
	*xpucfg_arr_pa_out = arr[AC_XPUCFG_ARRAY_IDX];
	*xpucfg_count_pa_out = arr[AC_XPUCFG_ARRAY_SIZE_IDX];

	qti_mmap_remove_dynamic_region((uintptr_t)arr_pa, map_size);

	return 0;
}

static uint32_t translate_rgs(uint64_t rg_pa, uint32_t nrg,
			      struct xpu4_rg *out, uint32_t max_out)
{
	uint32_t n = nrg;
	uint32_t i;
	int ret;
	const uint8_t *base;

	if (n > max_out) {
		WARN("ac_config: nrg=%u exceeds cap %u; truncating\n", n, max_out);
		n = max_out;
	}
	if (n == 0U || rg_pa == 0U) {
		return 0U;
	}

	ret = qti_mmap_add_dynamic_region((uintptr_t)rg_pa,
					  (size_t)n * ACMPU_RG_ENTRY_SIZE,
					  MT_RO_DATA | MT_SECURE);
	if (ret != 0) {
		VERBOSE("ac_config: rg array map failed (%d)\n", ret);
		return 0U;
	}

	base = (const uint8_t *)(uintptr_t)rg_pa;
	for (i = 0U; i < n; i++) {
		const uint8_t *e = base + ((size_t)i * ACMPU_RG_ENTRY_SIZE);

		out[i].rg_num = *(const uint16_t *)(e + ACMPU_RG_OFF_RGNUM);
		out[i].start = *(const uint64_t *)(e + ACMPU_RG_OFF_START);
		out[i].end = *(const uint64_t *)(e + ACMPU_RG_OFF_END);
		out[i].read_qads = *(const uint32_t *)(e + ACMPU_RG_OFF_READQADS);
		out[i].write_qads = *(const uint32_t *)(e + ACMPU_RG_OFF_WRITEQADS);
		out[i].flags = XPU4_RG_ENABLE;
	}

	qti_mmap_remove_dynamic_region((uintptr_t)rg_pa,
				       (size_t)n * ACMPU_RG_ENTRY_SIZE);

	return n;
}

/* Translate ACXpuCfg[] into struct xpu4_instance[]. Disabled entries
 * (status == XPU_DISABLED) are skipped entirely, including their RGs.
 */
static uint32_t translate_xpu_cfg_array(uint64_t arr_pa, uint32_t count,
					struct xpu4_instance *out,
					uint32_t max_out)
{
	uint32_t accepted = 0U;
	uint32_t i;
	int ret;
	const uint8_t *base;

	ret = qti_mmap_add_dynamic_region((uintptr_t)arr_pa,
					  (size_t)count * ACXPUCFG_ENTRY_SIZE,
					  MT_RO_DATA | MT_SECURE);
	if (ret != 0) {
		WARN("ac_config: xpucfg array map failed (%d)\n", ret);
		return 0U;
	}

	base = (const uint8_t *)(uintptr_t)arr_pa;

	for (i = 0U; i < count && accepted < max_out; i++) {
		const uint8_t *e = base + ((size_t)i * ACXPUCFG_ENTRY_SIZE);
		uint16_t status = *(const uint16_t *)(e + ACXPUCFG_OFF_STATUS);
		uint16_t nrg = *(const uint16_t *)(e + ACXPUCFG_OFF_NRG);
		uint64_t rg_pa = *(const uint64_t *)(e + ACXPUCFG_OFF_RG);
		struct xpu4_instance *inst;

		if (status == XPU_STATUS_DISABLED) {
			continue;
		}

		inst = &out[accepted];
		inst->base = (uintptr_t)
			(*(const uint32_t *)(e + ACXPUCFG_OFF_BASEADDR));
		inst->xpu_id = *(const uint32_t *)(e + ACXPUCFG_OFF_XPUID);
		inst->umr_perm = *(const uint32_t *)(e + ACXPUCFG_OFF_UMRPERM);
		inst->cfg_owner = *(const uint32_t *)(e + ACXPUCFG_OFF_CFGOWNER);
		inst->flags = XPU4_INST_SET_UMR | XPU4_INST_SET_CFGOWNER |
			      XPU4_INST_ERR_REPORT;
		inst->rgs = nord_ac_parsed_rgs[accepted];
		inst->nrg = translate_rgs(rg_pa, nrg, nord_ac_parsed_rgs[accepted],
					  AC_CONFIG_MAX_RG_PER_INST);

		accepted++;
	}

	qti_mmap_remove_dynamic_region((uintptr_t)arr_pa,
				       (size_t)count * ACXPUCFG_ENTRY_SIZE);

	return accepted;
}

const struct xpu4_instance *ac_config_lookup_xpu_cfg(uint32_t *count)
{
	struct nord_ac_config_smem *smem_item;
	size_t smem_size;
	uint64_t ac_config_pa;
	uint32_t num_entries;
	uint64_t globaldata_arr_pa;
	uint64_t xpucfg_arr_pa = 0U;
	uint64_t xpucfg_count_pa = 0U;
	uint32_t xpucfg_count;
	uint32_t accepted;
	int ret;

	*count = 0U;

	ret = qti_smem_lookup(QTI_SMEM_HOST_COMMON, SMEM_NORD_AC_CONFIG_ADDR,
			      QTI_SMEM_FLAG_NONE, (void **)&smem_item,
			      &smem_size);
	if (ret != 0 || smem_item == NULL || smem_size < sizeof(*smem_item)) {
		VERBOSE("ac_config: SMEM item not present (%d)\n", ret);
		return NULL;
	}

	ac_config_pa = smem_item->ac_config_pa;
	if (ac_config_pa == 0U) {
		return NULL;
	}

	if (read_global_header(ac_config_pa, &num_entries,
			       &globaldata_arr_pa) != 0) {
		return NULL;
	}

	if (num_entries <= AC_XPUCFG_ARRAY_SIZE_IDX ||
	    num_entries > AC_CONFIG_MAX_ENTRIES) {
		WARN("ac_config: bad uNumEntries=%u\n", num_entries);
		return NULL;
	}

	if (read_xpucfg_locators(globaldata_arr_pa, num_entries,
				 &xpucfg_arr_pa, &xpucfg_count_pa) != 0) {
		return NULL;
	}

	if (xpucfg_arr_pa == 0U || xpucfg_count_pa == 0U) {
		WARN("ac_config: missing XPUCFG array/size entry\n");
		return NULL;
	}

	if (read_u32_at_pa(xpucfg_count_pa, &xpucfg_count) != 0) {
		return NULL;
	}

	if (xpucfg_count == 0U || xpucfg_count > AC_CONFIG_MAX_INSTANCES) {
		WARN("ac_config: bad xpucfg count=%u\n", xpucfg_count);
		return NULL;
	}

	accepted = translate_xpu_cfg_array(xpucfg_arr_pa, xpucfg_count,
					   nord_ac_parsed_insts,
					   AC_CONFIG_MAX_INSTANCES);
	if (accepted == 0U) {
		WARN("ac_config: no usable XPU instances in DRAM policy\n");
		return NULL;
	}

	NOTICE("ac_config: parsed %u XPU4 instance(s) from DRAM policy\n",
	       accepted);
	*count = accepted;

	return nord_ac_parsed_insts;
}
