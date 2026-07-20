/*
 * Copyright (c) 2026 Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <drivers/qti/pdc/pdc_seq.h>

/*
 * Nord (SA8797P) AOSS PDC sequencer configuration.
 *
 * Matches the downstream Nord TZ config exactly (seq/cfg/nord/pdc_seq_cfg.c
 * and pdc_grp0_branch.h, "Generated from: NordAU V1.0, Version: 0x52783f1"):
 * no branch addresses/delays are statically programmed (AOP owns them), and
 * all 5 low-power modes leave cmds/length at NULL/0 - their microcode is
 * written by AOP at runtime via the branch-mask hidden-TCS command, not by
 * this driver at init time. (An earlier version of this file instead
 * fabricated CX-retention microcode borrowed from the lemans reference data
 * - that never matched what Nord's own firmware programs here and is not
 * used for anything in this codebase, since nothing here implements the
 * runtime low-power-mode-entry path that would consume start_addr/
 * branch_mask; see pdc_seq_set_lpm() downstream for that path.)
 *
 * mode_id values below mirror the downstream mode list order 1:1
 * (CX_MIN, CX_COL, CX_MIN_AOSS, CX_COL_AOSS, DeepSleep); branch_mask is not
 * tracked here since nothing in this driver consumes it.
 */

static uint8_t apps_branches[PDC_SEQ_BR_ADDR_REG_COUNT] = { 0 };
static uint32_t apps_delays[PDC_SEQ_DELAY_REG_COUNT] = { 0 };

static struct pdc_seq_cfg apps_pdc_cfg = {
	apps_branches, 0U,	/* Nord: AOP-programmed, no static branches */
	apps_delays, 0U,	/* Nord: AOP-programmed, no static delays */
};

static struct pdc_seq_mode apps_pdc_modes[] = {
	{ NULL, 0U, 1U, 0 }, /* mode_id 1: CX_MIN      (CX ret, no AOSS)   */
	{ NULL, 0U, 2U, 0 }, /* mode_id 2: CX_COL      (CX off, no AOSS)   */
	{ NULL, 0U, 3U, 0 }, /* mode_id 3: CX_MIN_AOSS (CX ret, with AOSS) */
	{ NULL, 0U, 4U, 0 }, /* mode_id 4: CX_COL_AOSS (CX off, with AOSS) */
	{ NULL, 0U, 5U, 0 }, /* mode_id 5: DeepSleep                       */
};

static struct pdc_seq pdc_seq_instance = {
	PDC_SEQ_APPS,
	&apps_pdc_cfg,
	apps_pdc_modes, 5U,
	0x200000U,	/* PDC offset from AOSS base                 */
	0x1900000U,	/* RSC offset from APSS_HM base (Nord RSC)   */
};

struct pdc_seq *g_pdc_seqs = &pdc_seq_instance;
uint32_t g_pdc_seq_count = 1U;
