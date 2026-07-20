/*
 * Copyright (c) 2015-2024, ARM Limited and Contributors. All rights reserved.
 * Copyright (c) 2018-2024, The Linux Foundation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/bl_common.h>
#include <drivers/arm/gicv3.h>

#include <platform.h>
#include <platform_def.h>
#include <qti_plat.h>

/* The GICv3 driver only needs to be initialized in EL3 */
static uintptr_t rdistif_base_addrs[PLATFORM_CORE_COUNT];

/*
 * Array of secure interrupts to be configured by the GIC driver. The contents
 * are architecture-specific (the interrupt IDs differ per SoC), so the table
 * and its length are supplied by the platform layer.
 */
extern const interrupt_prop_t qti_interrupt_props[];
extern const unsigned int qti_interrupt_props_num;

static gicv3_driver_data_t qti_gic_data = {
	.gicd_base = QTI_GICD_BASE,
	.gicr_base = QTI_GICR_BASE,
	.interrupt_props = qti_interrupt_props,
	.rdistif_num = PLATFORM_CORE_COUNT,
	.rdistif_base_addrs = rdistif_base_addrs,
	.mpidr_to_core_pos = plat_qti_core_pos_by_mpidr
};

void plat_qti_gic_driver_init(void)
{
	/*
	 * The interrupt property table is supplied by the platform layer; its
	 * length is an extern const (not a compile-time constant), so wire it
	 * in at runtime before initialising the driver.
	 */
	qti_gic_data.interrupt_props_num = qti_interrupt_props_num;

	/*
	 * The GICv3 driver is initialized in EL3 and does not need
	 * to be initialized again in SEL1. This is because the S-EL1
	 * can use GIC system registers to manage interrupts and does
	 * not need GIC interface base addresses to be configured.
	 */
	gicv3_driver_init(&qti_gic_data);
}

/******************************************************************************
 * ARM common helper to initialize the GIC. Only invoked by BL31
 *****************************************************************************/
void plat_qti_gic_init(void)
{
	unsigned int i;

	gicv3_distif_init();
	gicv3_rdistif_init(plat_my_core_pos());
	gicv3_cpuif_enable(plat_my_core_pos());

	/* Route secure spi interrupt to ANY. */
	for (i = 0; i < qti_interrupt_props_num; i++) {
		unsigned int int_id = qti_interrupt_props[i].intr_num;

		if (plat_ic_is_spi(int_id)) {
			gicv3_set_spi_routing(int_id, GICV3_IRM_ANY, 0x0);
		}
	}
}

void gic_set_spi_routing(unsigned int id, unsigned int irm, u_register_t target)
{
	gicv3_set_spi_routing(id, irm, target);
}

/******************************************************************************
 * ARM common helper to enable the GIC CPU interface
 *****************************************************************************/
void plat_qti_gic_cpuif_enable(void)
{
	gicv3_cpuif_enable(plat_my_core_pos());
}

/******************************************************************************
 * ARM common helper to disable the GIC CPU interface
 *****************************************************************************/
void plat_qti_gic_cpuif_disable(void)
{
	gicv3_cpuif_disable(plat_my_core_pos());
}

/******************************************************************************
 * ARM common helper to initialize the per-CPU redistributor interface in GICv3
 *****************************************************************************/
void plat_qti_gic_pcpu_init(void)
{
	gicv3_rdistif_init(plat_my_core_pos());
}

/******************************************************************************
 * ARM common helpers to power GIC redistributor interface
 *****************************************************************************/
void plat_qti_gic_redistif_on(void)
{
	gicv3_rdistif_on(plat_my_core_pos());
}

void plat_qti_gic_redistif_off(void)
{
	gicv3_rdistif_off(plat_my_core_pos());
}
