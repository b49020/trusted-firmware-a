/*
 * Copyright (c) 2026 Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <drivers/arm/gicv3.h>
#include <lib/mmio.h>

#include <common/debug.h>
#include <drivers/qti/cmd_db/cmd_db.h>
#include <drivers/qti/pwr_utils/pwr_utils_lvl.h>
#include <platform_def.h>
#include <qti_plat.h>

void qti_plat_bl31_setup_post(void)
{
	/*
	 * Mask TME_WDOG_EXPIRED (and defensively PDC_WDOG_EXPIRED) at the final
	 * MPM_PS_HOLD_MASK gate. The direct TME_WDOG disable does NOT prevent
	 * TME_WDOG_EXPIRED from being observed set - A/B boot testing confirms
	 * this mask is what actually stops the ~12s cold-boot PS_HOLD reset.
	 * Bring-up mitigation, not a safety-compliant fix: it masks the reset
	 * request rather than resolving why TME still asserts it.
	 */
	mmio_clrbits_32(QTI_MPM_PS_HOLD_MASK,
			QTI_MPM_PS_HOLD_MASK_TME_WDOG_EXPIRED |
			QTI_MPM_PS_HOLD_MASK_PDC_WDOG_EXPIRED);

	/*
	 * Bring up the APSS INTU so peripheral SPIs (UFS SPI265/INTID297, geni
	 * 615, RSC 61) reach the GIC-700. Downstream CPUSS sysini programs the
	 * per-SPI level/edge type; our stub did not, so level SPIs never
	 * delivered. Runs before bl31_plat_runtime_setup() enables G1NS SPI
	 * forwarding at the distributor.
	 */
	plat_intu_init();

	/*
	 * Build the SW-corner (vlvl) -> HW-level (hlvl) tables from the AOP
	 * command DB. cmd-db is already initialized by qti_pdc_init().
	 */
	pwr_utils_lvl_init();

	/*
	 * DIAGNOSTIC (temporary, bring-up): the Nord PDC wake TCS hardcodes the
	 * rail hlvls (cx=2 "min_svs", mx=2 "NOM", xo=3 "ON"). Log the values the
	 * AOP cmd-db actually maps for those corners so we can confirm the
	 * hardcoded indices are correct - a mismatch (or a "len=0", meaning
	 * cmd-db has no table for the rail) is a concrete lead on the RPMh
	 * ACTIVE-TCS wake handshake. Remove once the PDC config is switched to
	 * these derived values.
	 */
	NOTICE("pwr_utils: cx.lvl len=%u hlvl(MIN_SVS)=%d (pdc hardcodes 2)\n",
	       cmd_db_query_len("cx.lvl"),
	       pwr_utils_hlvl_named_resource("cx.lvl",
					     RAIL_VOLTAGE_LEVEL_MIN_SVS, NULL));
	NOTICE("pwr_utils: mx.lvl len=%u hlvl(NOM)=%d (pdc hardcodes 2)\n",
	       cmd_db_query_len("mx.lvl"),
	       pwr_utils_hlvl_named_resource("mx.lvl",
					     RAIL_VOLTAGE_LEVEL_NOM, NULL));
	NOTICE("pwr_utils: xo.lvl len=%u hlvl(ON)=%d (pdc hardcodes 3)\n",
	       cmd_db_query_len("xo.lvl"),
	       pwr_utils_hlvl_named_resource("xo.lvl",
					     0x80 /* XO_LEVEL_ON */, NULL));
}

/*******************************************************************************
 * Perform any platform specific runtime setup prior to cold boot exit
 * from BL31
 ******************************************************************************/
void bl31_plat_runtime_setup(void)
{
	/*
	 * Enable distributor forwarding of G1NS SPIs as the last cold-boot
	 * action, after all BL31 setup (secure SPIs) is complete.
	 * gicv3_distif_init() clears EnableGrp1NS and never restores it;
	 * this GIC-700 also resets it to 0, so it must be set explicitly here.
	 */
	mmio_setbits_32(QTI_GICD_BASE + GICD_CTLR, CTLR_ENABLE_G1NS_BIT);
	while ((mmio_read_32(QTI_GICD_BASE + GICD_CTLR) &
		GICD_CTLR_RWP_BIT) != 0U) {
	}
}
