/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>

#include <lib/psci/psci.h>
#include <plat/common/platform.h>

#include <qti_plat.h>

/*
 * Native PSCI / interrupt-dispatch hooks for the Wildcat (Nord) port. Unlike
 * hoya, the open Nord BL31 links no QTISECLIB, so these hooks are implemented
 * natively here rather than forwarded to a blob (see qti_plat.h).
 *
 * This base port brings up only the primary core; the secondary-core NCC
 * power-on sequence is added by the PSCI CPU_ON support. The remaining hooks
 * are no-ops: Nord advertises CPU standby (WFI) only, so the PSCI framework
 * never drives the power-off / suspend node paths.
 */

void plat_qti_pwr_domain_on(u_register_t mpidr, int core_pos)
{
	(void)mpidr;
	(void)core_pos;
}

void plat_qti_pwr_domain_on_finish(int core_pos, const uint8_t *states)
{
	(void)core_pos;
	(void)states;

	plat_qti_gic_pcpu_init();
}

void plat_qti_pwr_domain_off(const uint8_t *states)
{
	(void)states;
}

void plat_qti_pwr_domain_suspend(const uint8_t *states)
{
	(void)states;
}

void plat_qti_pwr_domain_suspend_finish(const uint8_t *states)
{
	(void)states;
}

int plat_qti_pwr_psci_init(uintptr_t warmboot_entry)
{
	(void)warmboot_entry;
	return PSCI_E_SUCCESS;
}

void plat_qti_invoke_unhandled_isr(uint32_t id, void *handle)
{
	(void)id;
	(void)handle;
}
