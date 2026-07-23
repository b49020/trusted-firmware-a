/*
 * Copyright (c) 2016-2021 Qualcomm Technologies, Inc. (QTI). All Rights Reserved.
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * pwr_utils_lvl.h - convert SW virtual corner/level (vlvl) to HW corner/level
 * (hlvl) and vice versa, using the per-resource aux-data tables published by
 * the AOP in the RPMh command DB (cmd-db).
 *
 * Ported from the downstream Qualcomm TrustZone driver
 * (core/power/utils/pwr_utils_lvl.c).
 */

#ifndef PWR_UTILS_LVL_H
#define PWR_UTILS_LVL_H

#include <stdint.h>

/*
 * SW voltage-rail corners (vlvl). These values are fixed for the lifetime of
 * the chip and are shared with the RPMh voltage-level ABI (voltage_level.h in
 * the downstream tree). Only the enum is needed here.
 */
enum rail_voltage_level {
	RAIL_VOLTAGE_LEVEL_OFF		= 0x0,
	RAIL_VOLTAGE_LEVEL_RET		= 0x10,
	RAIL_VOLTAGE_LEVEL_MIN_SVS	= 0x30,
	RAIL_VOLTAGE_LEVEL_LOW_SVS_D2	= 0x34,
	RAIL_VOLTAGE_LEVEL_LOW_SVS_D1	= 0x38,
	RAIL_VOLTAGE_LEVEL_LOW_SVS_D0	= 0x3C,
	RAIL_VOLTAGE_LEVEL_LOW_SVS	= 0x40,
	RAIL_VOLTAGE_LEVEL_LOW_SVS_P1	= 0x48,
	RAIL_VOLTAGE_LEVEL_LOW_SVS_L1	= 0x50,
	RAIL_VOLTAGE_LEVEL_LOW_SVS_L2	= 0x60,
	RAIL_VOLTAGE_LEVEL_SVS		= 0x80,
	RAIL_VOLTAGE_LEVEL_SVS_L0	= 0x90,
	RAIL_VOLTAGE_LEVEL_SVS_L1	= 0xC0,
	RAIL_VOLTAGE_LEVEL_SVS_L2	= 0xE0,
	RAIL_VOLTAGE_LEVEL_NOM		= 0x100,
	RAIL_VOLTAGE_LEVEL_NOM_L0	= 0x120,
	RAIL_VOLTAGE_LEVEL_NOM_L1	= 0x140,
	RAIL_VOLTAGE_LEVEL_NOM_L2	= 0x150,
	RAIL_VOLTAGE_LEVEL_TUR		= 0x180,
	RAIL_VOLTAGE_LEVEL_TUR_L0	= 0x190,
	RAIL_VOLTAGE_LEVEL_TUR_L1	= 0x1A0,
	RAIL_VOLTAGE_LEVEL_TUR_L2	= 0x1B0,
	RAIL_VOLTAGE_LEVEL_TUR_L3	= 0x1C0,
	RAIL_VOLTAGE_LEVEL_NUM_LEVELS	= 25,
	RAIL_VOLTAGE_LEVEL_INVALID	= -1,
	RAIL_VOLTAGE_LEVEL_OVERLIMIT	= -2,
};

/*
 * pwr_utils_lvl_init - discover per-resource level tables from cmd-db.
 * Must be called once during BL31 setup, after cmd-db is available.
 */
void pwr_utils_lvl_init(void);

/* pwr_utils_mol_init - discover per-resource minimum-operating-levels. */
void pwr_utils_mol_init(void);

/*
 * pwr_utils_lvl_resource_idx - resolve a resource name (e.g. "cx.lvl") to an
 * opaque index for the pwr_utils_hlvl / pwr_utils_vlvl fast paths.
 * Returns index (>=0) or -1 if not found.
 */
int pwr_utils_lvl_resource_idx(const char *res_name);

/*
 * pwr_utils_hlvl - map a SW corner (vlvl) to the HW level (hlvl) for the
 * resource at @resource_idx. If @mapped_vlvl is non-NULL it receives the vlvl
 * actually used (== vlvl if exact, > vlvl if rounded up, OVERLIMIT if too big).
 * Returns hlvl (>=0) or -1 on error.
 */
int pwr_utils_hlvl(int resource_idx, int vlvl, int *mapped_vlvl);

/* pwr_utils_hlvl_named_resource - as pwr_utils_hlvl, by resource name. */
int pwr_utils_hlvl_named_resource(const char *resource, int vlvl,
				  int *mapped_vlvl);

/* pwr_utils_resource_lvls_count - number of hlvls for @resource_idx, or -1. */
int pwr_utils_resource_lvls_count(int resource_idx);

/* pwr_utils_named_resource_lvls_count - as above, by resource name. */
int pwr_utils_named_resource_lvls_count(const char *resource);

/*
 * pwr_utils_vlvl - map a HW level (hlvl) back to its SW corner (vlvl) for the
 * resource at @resource_idx. Returns vlvl or RAIL_VOLTAGE_LEVEL_INVALID.
 */
int pwr_utils_vlvl(int resource_idx, int hlvl);

/* pwr_utils_vlvl_named_resource - as pwr_utils_vlvl, by resource name. */
int pwr_utils_vlvl_named_resource(const char *resource, int hlvl);

/*
 * pwr_utils_mol_resource_idx - resolve a MOL resource name (e.g. "cx.mol") to
 * an opaque index. Returns index (>=0) or -1.
 */
int pwr_utils_mol_resource_idx(const char *res_name);

/*
 * pwr_utils_mol_hlvl - HW level of the minimum operating level (MOL) for the
 * MOL resource at @resource_idx. Returns hlvl (>=0) or an error < 0.
 */
int pwr_utils_mol_hlvl(int resource_idx);

/* pwr_utils_mol_hlvl_named_resource - as pwr_utils_mol_hlvl, by name. */
int pwr_utils_mol_hlvl_named_resource(const char *resource);

#endif /* PWR_UTILS_LVL_H */
