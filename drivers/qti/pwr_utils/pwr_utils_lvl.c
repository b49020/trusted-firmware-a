/*
 * Copyright (c) 2016-2021 Qualcomm Technologies, Inc. (QTI). All Rights Reserved.
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Convert SW virtual corner/level (vlvl) to HW corner/level (hlvl) and vice
 * versa, using the per-resource aux-data tables published by the AOP in the
 * RPMh command DB (cmd-db).
 *
 * Ported from the downstream Qualcomm TrustZone driver
 * (core/power/utils/pwr_utils_lvl.c). TF-A adaptations: fixed static level
 * buffers instead of tzbsp_malloc, assert()/graceful returns instead of
 * CORE_VERIFY, and standard TF-A types.
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <common/debug.h>
#include <drivers/qti/cmd_db/cmd_db.h>
#include <drivers/qti/pwr_utils/pwr_utils_lvl.h>

/*
 * The cmd-db aux-data length is a uint8 (the API caps a single item's aux at
 * 255 bytes), i.e. at most 127 uint16 levels. Size the per-resource level
 * buffer accordingly so no dynamic allocation is needed in BL31.
 */
#define PWR_UTILS_MAX_LVLS	128U

struct pwr_utils_lvl_res {
	const char *name;	/* resource name e.g. "cx.lvl"            */
	uint16_t *vlvls;	/* supported vlvls, indexed by hlvl        */
	size_t count;		/* number of valid vlvls                   */
};

struct pwr_utils_mol_res {
	const char *name;	/* resource name e.g. "cx.mol"             */
	uint16_t mol;		/* minimum operating level (vlvl)          */
};

static struct pwr_utils_lvl_res *g_res;
static size_t g_res_count;

static struct pwr_utils_mol_res *g_res_mol;
static size_t g_res_mol_count;

/*
 * Target rail resources. Kept here (not in devcfg) to keep the footprint
 * small, matching the downstream driver.
 */
static struct pwr_utils_lvl_res res_names[] = {
	{ "cx.lvl",   NULL, 0 },
	{ "mx.lvl",   NULL, 0 },
	{ "ebi.lvl",  NULL, 0 },
	{ "lcx.lvl",  NULL, 0 },
	{ "lmx.lvl",  NULL, 0 },
	{ "gfx.lvl",  NULL, 0 },
	{ "mss.lvl",  NULL, 0 },
	{ "ddr.lvl",  NULL, 0 },
	{ "xo.lvl",   NULL, 0 },
	{ "mmcx.lvl", NULL, 0 },
	{ "qlnk.lvl", NULL, 0 },
	{ "nsp.lvl",  NULL, 0 },
	{ "mxc.lvl",  NULL, 0 },
	{ "gmxc.lvl", NULL, 0 },
	{ "lnoc.lvl", NULL, 0 },
	{ "nsp2.lvl", NULL, 0 },
	{ "dcx.lvl",  NULL, 0 },
	{ "gfx1.lvl", NULL, 0 },
	{ "nsp0.lvl", NULL, 0 },
	{ "nsp1.lvl", NULL, 0 },
	{ "nsp3.lvl", NULL, 0 },
};

static struct pwr_utils_mol_res res_mol_names[] = {
	{ "cx.mol",   0 },
	{ "mx.mol",   0 },
	{ "ebi.mol",  0 },
	{ "lcx.mol",  0 },
	{ "lmx.mol",  0 },
	{ "gfx.mol",  0 },
	{ "mss.mol",  0 },
	{ "ddr.mol",  0 },
	{ "xo.mol",   0 },
	{ "mmcx.mol", 0 },
	{ "nsp.mol",  0 },
	{ "mxc.mol",  0 },
	{ "gmxc.mol", 0 },
	{ "lnoc.mol", 0 },
	{ "nsp2.mol", 0 },
	{ "dcx.mol",  0 },
	{ "gfx1.mol", 0 },
	{ "nsp0.mol", 0 },
	{ "nsp1.mol", 0 },
	{ "nsp3.mol", 0 },
};

/* Backing storage for each resource's level table (no heap in BL31). */
static uint16_t g_lvl_storage[ARRAY_SIZE(res_names)][PWR_UTILS_MAX_LVLS];

void pwr_utils_lvl_init(void)
{
	uint32_t i, j;
	struct pwr_utils_lvl_res *res;
	const char *res_name;
	uint32_t data_len;
	uint8_t aux_len;

	g_res = res_names;
	g_res_count = ARRAY_SIZE(res_names);

	for (i = 0U; i < g_res_count; i++) {
		res = &g_res[i];
		res_name = res->name;
		data_len = cmd_db_query_len(res_name);

		if (data_len == 0U) {
			/* Not published by the AOP - leave count 0. */
			continue;
		}

		/* Clamp to the static buffer size. */
		if (data_len > PWR_UTILS_MAX_LVLS * sizeof(uint16_t)) {
			data_len = PWR_UTILS_MAX_LVLS * sizeof(uint16_t);
		}

		res->vlvls = g_lvl_storage[i];
		aux_len = (uint8_t)data_len;
		if (cmd_db_query_aux_data(res_name, &aux_len,
					  (uint8_t *)res->vlvls) != 0) {
			res->vlvls = NULL;
			continue;
		}

		/* Count valid levels (a trailing 0 vlvl, except index 0, ends). */
		for (j = 0U; j < ((uint32_t)aux_len / sizeof(uint16_t)); j++) {
			if ((res->vlvls[j] == 0U) && (j != 0U)) {
				break;
			}
		}
		res->count = j;
	}

	pwr_utils_mol_init();
}

void pwr_utils_mol_init(void)
{
	uint32_t i;
	struct pwr_utils_mol_res *res;
	const char *res_name;
	uint32_t data_len;
	uint8_t aux_len;

	g_res_mol = res_mol_names;
	g_res_mol_count = ARRAY_SIZE(res_mol_names);

	for (i = 0U; i < g_res_mol_count; i++) {
		res = &g_res_mol[i];
		res_name = res->name;
		data_len = cmd_db_query_len(res_name);

		if (data_len == 0U) {
			continue;
		}

		aux_len = (uint8_t)sizeof(res->mol);
		(void)cmd_db_query_aux_data(res_name, &aux_len,
					    (uint8_t *)&res->mol);
	}
}

int pwr_utils_lvl_resource_idx(const char *res_name)
{
	uint32_t i;

	if ((res_name == NULL) || (g_res == NULL) || (g_res_count == 0U)) {
		return -1;
	}

	for (i = 0U; i < g_res_count; i++) {
		if (strcmp(g_res[i].name, res_name) == 0) {
			return (int)i;
		}
	}

	return -1;
}

int pwr_utils_hlvl(int resource_idx, int vlvl, int *mapped_vlvl)
{
	uint32_t i;
	int temp;
	struct pwr_utils_lvl_res *res;

	mapped_vlvl = (mapped_vlvl == NULL) ? &temp : mapped_vlvl;

	if ((resource_idx < 0) || ((size_t)resource_idx >= g_res_count) ||
	    (vlvl < 0)) {
		*mapped_vlvl = RAIL_VOLTAGE_LEVEL_INVALID;
		return -1;
	}

	res = &g_res[resource_idx];

	for (i = 0U; i < res->count; i++) {
		if (res->vlvls[i] >= vlvl) {
			*mapped_vlvl = res->vlvls[i];
			return (int)i;
		}
	}

	/* Requested vlvl exceeds the max supported vlvl for this resource. */
	*mapped_vlvl = RAIL_VOLTAGE_LEVEL_OVERLIMIT;
	return -1;
}

int pwr_utils_hlvl_named_resource(const char *resource, int vlvl,
				  int *mapped_vlvl)
{
	int res_idx = pwr_utils_lvl_resource_idx(resource);

	/* pwr_utils_hlvl validates res_idx. */
	return pwr_utils_hlvl(res_idx, vlvl, mapped_vlvl);
}

int pwr_utils_resource_lvls_count(int resource_idx)
{
	if ((resource_idx < 0) || ((size_t)resource_idx >= g_res_count)) {
		return -1;
	}

	return (int)g_res[resource_idx].count;
}

int pwr_utils_named_resource_lvls_count(const char *resource)
{
	return pwr_utils_resource_lvls_count(
		pwr_utils_lvl_resource_idx(resource));
}

int pwr_utils_vlvl(int resource_idx, int hlvl)
{
	struct pwr_utils_lvl_res *res;

	if ((resource_idx < 0) || ((size_t)resource_idx >= g_res_count)) {
		return RAIL_VOLTAGE_LEVEL_INVALID;
	}

	res = &g_res[resource_idx];

	return ((hlvl >= 0) && ((size_t)hlvl < res->count)) ?
		(int)res->vlvls[hlvl] : RAIL_VOLTAGE_LEVEL_INVALID;
}

int pwr_utils_vlvl_named_resource(const char *resource, int hlvl)
{
	int res_idx = pwr_utils_lvl_resource_idx(resource);

	return pwr_utils_vlvl(res_idx, hlvl);
}

int pwr_utils_mol_resource_idx(const char *res_name)
{
	uint32_t i;

	if ((res_name == NULL) || (g_res_mol == NULL) ||
	    (g_res_mol_count == 0U)) {
		return -1;
	}

	for (i = 0U; i < g_res_mol_count; i++) {
		if (strcmp(g_res_mol[i].name, res_name) == 0) {
			return (int)i;
		}
	}

	return -1;
}

int pwr_utils_mol_hlvl(int resource_idx)
{
	struct pwr_utils_mol_res *res;
	int mol_vlvl;
	int mapped_vlvl = 0;

	if ((resource_idx < 0) || ((size_t)resource_idx >= g_res_mol_count)) {
		return RAIL_VOLTAGE_LEVEL_INVALID;
	}

	res = &g_res_mol[resource_idx];
	mol_vlvl = (int)res->mol;

	return pwr_utils_hlvl(resource_idx, mol_vlvl, &mapped_vlvl);
}

int pwr_utils_mol_hlvl_named_resource(const char *resource)
{
	int res_idx = pwr_utils_mol_resource_idx(resource);

	return pwr_utils_mol_hlvl(res_idx);
}
