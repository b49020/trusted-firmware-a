/*
 * Copyright (c) 2026 Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <drivers/qti/pdc/pdc_tcs.h>
#include <drivers/qti/pdc/tcs_resource.h>
#include <drivers/qti/pwr_utils/pwr_utils_lvl.h>

#include <common/debug.h>

/*
 * Nord (SA8797P) APSS TCS resource configuration.
 *
 * The Nord TrustZone tree ships no nord-specific pdc/tcs config; the CX/MX/XO/
 * VRM resource levels below are the SoC power-architecture levels shared with
 * the lemans reference data (and match the lemans TrustZone tcs_resource.c).
 */

#define TCS_RESOURCE_CX		"cx.lvl"
#define TCS_RESOURCE_MX		"mx.lvl"
#define TCS_RESOURCE_XO		"xo.lvl"
#define TCS_RESOURCE_SOC	"vrm.soc"

#define RES_CX_OFF	0U
#define RES_CX_RET	1U
#define RES_CX_MOL	2U	/* SVS3 (min_svs) */

#define RES_MX_RET	1U
#define RES_MX_MOL	2U	/* NOM */

#define RES_XO_OFF	0U
#define RES_XO_MOL	3U	/* ON */

#define VRM_SOC_OFF	0U
#define VRM_SOC_ON	1U

struct pdc_tcs_resource g_pdc_resource_list[TCS_TOTAL_RESOURCE_NUM] = {
	{ TCS_RESOURCE_CX  },
	{ TCS_RESOURCE_MX  },
	{ TCS_RESOURCE_XO  },
	{ TCS_RESOURCE_SOC },
};

struct pdc_tcs_config g_pdc_tcs_config[TCS_NUM_TOTAL][NUM_COMMANDS_PER_TCS] = {
	/* TCS 0 - Sleep: CX retention */
	{
		{ { RES_IDX_XO  }, { RES_XO_OFF, TCS_CFG_OPT_NONE, 0U } },
		{ { RES_IDX_CX  }, { RES_CX_RET, TCS_CFG_OPT_NONE, 0U } },
		{ { RES_IDX_MX  }, { RES_MX_RET, TCS_CFG_OPT_NONE, 0U } },
		{ { RES_IDX_SOC }, { VRM_SOC_OFF, TCS_CFG_OPT_NONE, 0U } },
	},
	/* TCS 1 - Sleep: CX off */
	{
		{ { RES_IDX_XO  }, { RES_XO_OFF, TCS_CFG_OPT_NONE, 0U } },
		{ { RES_IDX_CX  }, { RES_CX_OFF, TCS_CFG_OPT_NONE, 0U } },
		{ { RES_IDX_MX  }, { RES_MX_RET, TCS_CFG_OPT_NONE, 0U } },
		{ { RES_IDX_SOC }, { VRM_SOC_OFF, TCS_CFG_OPT_NONE, 0U } },
	},
	/* TCS 2 - Sleep: unused (matches DV/VI sequences) */
	{
		{ { RES_IDX_XO  }, { RES_XO_OFF, TCS_CFG_OPT_NONE, 0U } },
		{ { RES_IDX_CX  }, { RES_CX_OFF, TCS_CFG_OPT_NONE, 0U } },
		{ { RES_IDX_MX  }, { RES_MX_RET, TCS_CFG_OPT_NONE, 0U } },
		{ { RES_IDX_SOC }, { VRM_SOC_OFF, TCS_CFG_OPT_NONE, 0U } },
	},
	/* TCS 3 - Wake: resources to MOL */
	{
		{ { RES_IDX_MX  }, { RES_MX_MOL, TCS_CFG_OPT_CMD_RESP_REQ, 0U } },
		{ { RES_IDX_CX  }, { RES_CX_MOL, TCS_CFG_OPT_CMD_RESP_REQ, 0U } },
		{ { RES_IDX_XO  }, { RES_XO_MOL, TCS_CFG_OPT_CMD_RESP_REQ, 0U } },
		{ { RES_IDX_SOC }, { VRM_SOC_ON,  TCS_CFG_OPT_CMD_RESP_REQ, 0U } },
	},
};

/*
 * Map a wake-TCS resource index to its MOL command-DB resource name. Only the
 * voltage rails have a per-chip MOL entry; XO uses a fixed "always on" level
 * and VRM.SOC is a simple on/off, so both keep their static values.
 */
static const char *nord_mol_res_name(uint8_t res_idx)
{
	switch (res_idx) {
	case RES_IDX_CX:
		return "cx.mol";
	case RES_IDX_MX:
		return "mx.mol";
	default:
		return NULL;
	}
}

/*
 * Nord PDC wake-level resolution.
 *
 * The wake TCS above votes each rail to its minimum operating level (MOL).
 * The CX/MX MOL hlvls were historically hardcoded (2/2) but the AOP command
 * DB maps min_svs/NOM to a different hlvl on this silicon (observed 3/3), so a
 * hardcoded index under-volts CX/MX on every PDC wake. Derive the correct hlvl
 * from the "<rail>.mol" command-DB resources via pwr_utils; fall back to the
 * static value if the lookup is unavailable (e.g. command DB not populated).
 */
void pdc_tcs_plat_resolve_levels(void)
{
	uint32_t n_cmd;
	struct pdc_tcs_config *cmd;
	const char *mol_name;
	int hlvl;

	pwr_utils_lvl_init();

	for (n_cmd = 0U; n_cmd < NUM_COMMANDS_PER_TCS; n_cmd++) {
		cmd = &g_pdc_tcs_config[TCS_NUM_WAKE0][n_cmd];

		mol_name = nord_mol_res_name(cmd->cmd.index);
		if (mol_name == NULL) {
			continue;
		}

		hlvl = pwr_utils_mol_hlvl_named_resource(mol_name);
		if (hlvl < 0) {
			WARN("pdc: %s MOL hlvl unavailable; keeping static %u\n",
			     mol_name, cmd->data.res_val);
			continue;
		}

		if ((uint32_t)hlvl != cmd->data.res_val) {
			NOTICE("pdc: %s wake hlvl %u -> %d (from cmd-db)\n",
			       mol_name, cmd->data.res_val, hlvl);
		}
		cmd->data.res_val = (uint32_t)hlvl;
	}
}
