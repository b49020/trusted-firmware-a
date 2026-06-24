/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>

#include <arch_helpers.h>
#include <common/debug.h>
#include <drivers/delay_timer.h>
#include <lib/mmio.h>
#include <lib/psci/psci.h>
#include <plat/common/platform.h>

#include <cpucp.h>
#include <qti_plat.h>

/*
 * Native direct-register CPU and L3 power-on sequence for lemans (qcs9075).
 *
 * This implements the secondary-core cold-boot bringup that was previously
 * performed by qtiseclib's PSCI node machinery, using the raw APSS IPM register
 * sequence. The low-power-mode / suspend node handling (PCU/RSC/PDC) is not
 * ported here.
 */

/* Per-core APSS IPM register block. */
#define APSS_CPU_IPM_REG_BASE			0x18000000U
#define APSS_CPU_IPM_REG_OFFSET			0x10000U
#define APSS_CPU_IPM_REG(core)			(APSS_CPU_IPM_REG_BASE + \
						 ((core) * APSS_CPU_IPM_REG_OFFSET))

#define CPU_HEAD_SWITCH_CTL(core)		(APSS_CPU_IPM_REG(core) + 0x08U)
#define CPU_SEQ_FORCE_PWR_CTL_EN(core)		(APSS_CPU_IPM_REG(core) + 0x1cU)
#define CPU_SEQ_FORCE_PWR_CTL_VAL(core)		(APSS_CPU_IPM_REG(core) + 0x20U)
#define CPU_PCHANNEL_FSM_CTL(core)		(APSS_CPU_IPM_REG(core) + 0x44U)

/*
 * APSS cluster (L3/DSU) IPM alias register block (APSS_ALIAS_1). Used for the
 * gold-cluster L3 turn-on and memory-repair sequences.
 */
#define APSS_CL_IPM_REG_BASE			0x18090000U

#define L3_SEQ_FORCE_PWR_CTL_EN			(APSS_CL_IPM_REG_BASE + 0x1cU)
#define L3_SEQ_FORCE_PWR_CTL_VAL		(APSS_CL_IPM_REG_BASE + 0x20U)
#define L3_SEQ_STS1				(APSS_CL_IPM_REG_BASE + 0x38U)
#define CL_PCHANNEL_FSM_CTL			(APSS_CL_IPM_REG_BASE + 0x44U)
#define GOLD_PLL_SEQ_FORCE_PWR_CTL_EN		(APSS_CL_IPM_REG_BASE + 0xecU)
#define GOLD_PLL_SEQ_FORCE_PWR_CTL_VAL		(APSS_CL_IPM_REG_BASE + 0xf0U)
#define GOLD_PLL_SEQ_STS1			(APSS_CL_IPM_REG_BASE + 0xfcU)

#define L3_SEQ_STS1_MEM_REPAIR_DONE		0x40000U
#define L3_SEQ_FORCE_PWR_CTL_MEM_REPAIR		0x2000000U
#define GOLD_PLL_SEQ_STS1_MEM_REPAIR_DONE	0x40U
#define GOLD_PLL_SEQ_FORCE_PWR_CTL_MEM_REPAIR	0x4000U

/*
 * Gold-cluster (APC1) SAW4 AVS rail. The qcs9075 SAW4 instance for the gold
 * rail is at 0x18101000, with the AVS register region at +0x800 and the status
 * region at +0xc00 (see qtiseclib HAL_avs_SecondaryRailInit / saw_v4.c).
 */
#define GOLD_SAW4_BASE				0x18101000U
#define GOLD_SAW4_VCTL				(GOLD_SAW4_BASE + 0x900U)
#define GOLD_SAW4_AVS_CTL			(GOLD_SAW4_BASE + 0x904U)
#define GOLD_SAW4_PMIC_STS			(GOLD_SAW4_BASE + 0xc18U)

#define SAW4_AVS_CTL_EN				0x1U
#define SAW4_AVS_CTL_CTL_SEL			0x10U

#define SAW4_PMIC_STS_STATE_MASK		0x30000U
#define SAW4_PMIC_STS_CURR_DATA_MASK		0xffffU

/*
 * SAW4_VCTL encodes SIZE[20], ADR_IDX[18:16] and PMIC_DATA[15:0]. The enable
 * write uses the 8-bit EN address index (3) with data 0x80; the voltage write
 * uses the 16-bit VCTL address index (0) with the boot voltage as data.
 */
#define SAW4_VCTL_ENABLE			0x30080U
#define GOLD_SAW4_BOOT_VOLTAGE			828U
#define SAW4_VCTL_SET_VOLTAGE			(0x100000U | GOLD_SAW4_BOOT_VOLTAGE)

#define SAW4_PMIC_WRITE_RETRY			200U

/* The first gold-cluster (DSU1) core in the lemans CPU topology. */
#define QTI_FIRST_GOLD_CORE			4

/*
 * Time to let the gold PLL/cluster clock settle after the one-time cold boot
 * before the first gold core is released from reset (see gold_cluster_cold_boot).
 */
#define GOLD_CLUSTER_SETTLE_US			500U

/*
 * Repair the L3 memories of the gold cluster. Performed once, before the first
 * gold core is powered on.
 */
static void l3_memory_repair(void)
{
	uint32_t val;

	val = mmio_read_32(L3_SEQ_FORCE_PWR_CTL_VAL);
	mmio_write_32(L3_SEQ_FORCE_PWR_CTL_VAL,
		      val | L3_SEQ_FORCE_PWR_CTL_MEM_REPAIR);

	val = mmio_read_32(L3_SEQ_FORCE_PWR_CTL_EN);
	mmio_write_32(L3_SEQ_FORCE_PWR_CTL_EN,
		      val | L3_SEQ_FORCE_PWR_CTL_MEM_REPAIR);

	while ((mmio_read_32(L3_SEQ_STS1) & L3_SEQ_STS1_MEM_REPAIR_DONE) == 0U) {
	}

	val = mmio_read_32(L3_SEQ_FORCE_PWR_CTL_VAL);
	mmio_write_32(L3_SEQ_FORCE_PWR_CTL_VAL,
		      val & ~L3_SEQ_FORCE_PWR_CTL_MEM_REPAIR);

	val = mmio_read_32(L3_SEQ_FORCE_PWR_CTL_EN);
	mmio_write_32(L3_SEQ_FORCE_PWR_CTL_EN,
		      val & ~L3_SEQ_FORCE_PWR_CTL_MEM_REPAIR);
}

/* Execute the gold-cluster L3 turn-on sequence. Performed once. */
static void l3_cold_boot(void)
{
	mmio_write_32(L3_SEQ_FORCE_PWR_CTL_EN, 0x0U);
	mmio_write_32(L3_SEQ_FORCE_PWR_CTL_VAL, 0x010801a2U);
	mmio_write_32(L3_SEQ_FORCE_PWR_CTL_VAL, 0x01080082U);
	mmio_write_32(L3_SEQ_FORCE_PWR_CTL_VAL, 0x0108008aU);
	udelay(20);

	mmio_write_32(L3_SEQ_FORCE_PWR_CTL_VAL, 0x0108008eU);
	udelay(20);

	mmio_write_32(L3_SEQ_FORCE_PWR_CTL_VAL, 0x0108009eU);
	udelay(4);

	mmio_write_32(L3_SEQ_FORCE_PWR_CTL_VAL, 0x0108008eU);
	mmio_write_32(L3_SEQ_FORCE_PWR_CTL_VAL, 0x0108008cU);
	udelay(4);

	mmio_write_32(CL_PCHANNEL_FSM_CTL, 0x2000001U);
	mmio_write_32(L3_SEQ_FORCE_PWR_CTL_VAL, 0x8004cU);
	mmio_write_32(CL_PCHANNEL_FSM_CTL, 0x2000000U);
}

/*
 * Repair the gold-cluster PLL memories. Performed once, before the first gold
 * core is powered on.
 */
static void cpu_memory_repair(void)
{
	uint32_t val;

	val = mmio_read_32(GOLD_PLL_SEQ_FORCE_PWR_CTL_EN);
	mmio_write_32(GOLD_PLL_SEQ_FORCE_PWR_CTL_EN,
		      val | GOLD_PLL_SEQ_FORCE_PWR_CTL_MEM_REPAIR);

	val = mmio_read_32(GOLD_PLL_SEQ_FORCE_PWR_CTL_VAL);
	mmio_write_32(GOLD_PLL_SEQ_FORCE_PWR_CTL_VAL,
		      val | GOLD_PLL_SEQ_FORCE_PWR_CTL_MEM_REPAIR);

	while ((mmio_read_32(GOLD_PLL_SEQ_STS1) &
		GOLD_PLL_SEQ_STS1_MEM_REPAIR_DONE) == 0U) {
	}

	val = mmio_read_32(GOLD_PLL_SEQ_FORCE_PWR_CTL_VAL);
	mmio_write_32(GOLD_PLL_SEQ_FORCE_PWR_CTL_VAL,
		      val & ~GOLD_PLL_SEQ_FORCE_PWR_CTL_MEM_REPAIR);

	val = mmio_read_32(GOLD_PLL_SEQ_FORCE_PWR_CTL_EN);
	mmio_write_32(GOLD_PLL_SEQ_FORCE_PWR_CTL_EN,
		      val & ~GOLD_PLL_SEQ_FORCE_PWR_CTL_MEM_REPAIR);
}

/*
 * Wait for the gold-cluster SAW4 PMIC write to complete. Returns once the PMIC
 * state machine is idle (and, when requested, the current PMIC data matches the
 * expected value), or after the retry budget is exhausted.
 */
static void saw4_pmic_wait(uint32_t expect_data, bool check_data)
{
	unsigned int retry = SAW4_PMIC_WRITE_RETRY;

	while (retry-- > 0U) {
		uint32_t sts = mmio_read_32(GOLD_SAW4_PMIC_STS);

		if (((sts & SAW4_PMIC_STS_STATE_MASK) == 0U) &&
		    (!check_data ||
		     ((sts & SAW4_PMIC_STS_CURR_DATA_MASK) == expect_data))) {
			return;
		}

		udelay(2);
	}
}

/*
 * Enable the gold-cluster (APC1) SAW4 AVS rail and drive it to its boot
 * voltage. This mirrors qtiseclib's HAL_avs_SecondaryRailInit(); without it the
 * gold cluster has no supply and its cores never come out of reset.
 */
static void set_gold_cluster_voltage(void)
{
	uint32_t val;

	/* Let the SAW hardware adjust the rail voltage. */
	val = mmio_read_32(GOLD_SAW4_AVS_CTL);
	mmio_write_32(GOLD_SAW4_AVS_CTL, val | SAW4_AVS_CTL_EN);

	val = mmio_read_32(GOLD_SAW4_AVS_CTL);
	mmio_write_32(GOLD_SAW4_AVS_CTL, val | SAW4_AVS_CTL_CTL_SEL);

	/* Enable the PMIC rail and wait for warm-up. */
	mmio_write_32(GOLD_SAW4_VCTL, SAW4_VCTL_ENABLE);
	saw4_pmic_wait(0U, false);
	udelay(90);

	/* Restore the rail to its boot voltage. */
	mmio_write_32(GOLD_SAW4_VCTL, SAW4_VCTL_SET_VOLTAGE);
	saw4_pmic_wait(GOLD_SAW4_BOOT_VOLTAGE, true);
}

/* One-time gold-cluster (DSU1) bringup, run before its first core powers on. */
static void gold_cluster_cold_boot(void)
{
	static bool gold_cluster_booted;

	if (gold_cluster_booted) {
		return;
	}

	set_gold_cluster_voltage();
	l3_memory_repair();
	l3_cold_boot();
	cpu_memory_repair();

	/*
	 * The cold-boot sequence above only polls for the L3 and gold-PLL
	 * "sequence done" / "memory repair done" status, not for the gold PLL
	 * to actually lock and drive the cluster clock. Releasing the first
	 * gold core (the only one that runs this cold boot) before its clock is
	 * running leaves it stuck: PSCI CPU_ON reports success but the core
	 * never starts executing, so the kernel times it out and only 7 of the
	 * 8 CPUs come online. Let the PLL/clock settle before the reset
	 * sequence releases the core. This runs only on the one-time cold-boot
	 * path, so it does not affect the later gold cores or steady state.
	 */
	udelay(GOLD_CLUSTER_SETTLE_US);

	gold_cluster_booted = true;
}

/*
 * qti_pwr_domain_on - power on a secondary core using the raw APSS IPM reset
 * sequence.
 */
void qti_pwr_domain_on(u_register_t mpidr, int core_pos)
{
	(void)mpidr;

	/* Bring up the gold cluster before powering on its first core. */
	if (core_pos >= QTI_FIRST_GOLD_CORE) {
		gold_cluster_cold_boot();
	}

	/* Program skew between en_few and en_rest. */
	mmio_write_32(CPU_HEAD_SWITCH_CTL(core_pos), 0x28U);

	/* Clear power-control enables. */
	mmio_write_32(CPU_SEQ_FORCE_PWR_CTL_EN(core_pos), 0x0U);

	/* Close the core logic head switch. */
	mmio_write_32(CPU_SEQ_FORCE_PWR_CTL_VAL(core_pos), 0x642U);
	udelay(2);

	/* Deassert core memory and logic clamp. */
	mmio_write_32(CPU_SEQ_FORCE_PWR_CTL_VAL(core_pos), 0x402U);

	/* Deassert core memory slp_nret_n and slp_ret_n. */
	mmio_write_32(CPU_SEQ_FORCE_PWR_CTL_VAL(core_pos), 0x40aU);
	udelay(4);
	mmio_write_32(CPU_SEQ_FORCE_PWR_CTL_VAL(core_pos), 0x40eU);
	udelay(4);

	/* Assert and deassert wl_en_clk. */
	mmio_write_32(CPU_SEQ_FORCE_PWR_CTL_VAL(core_pos), 0x50eU);
	udelay(2);
	mmio_write_32(CPU_SEQ_FORCE_PWR_CTL_VAL(core_pos), 0x40eU);

	/* Deassert clock off. */
	mmio_write_32(CPU_SEQ_FORCE_PWR_CTL_VAL(core_pos), 0x40cU);
	udelay(2);

	/* Assert the core P-channel power-up request. */
	mmio_write_32(CPU_PCHANNEL_FSM_CTL(core_pos), 0x1U);

	/* Deassert core reset. */
	mmio_write_32(CPU_SEQ_FORCE_PWR_CTL_VAL(core_pos), 0x43cU);

	/* Deassert the core P-channel power-up request. */
	mmio_write_32(CPU_PCHANNEL_FSM_CTL(core_pos), 0x0U);

	/* Assert OSM core active. */
	mmio_write_32(CPU_SEQ_FORCE_PWR_CTL_VAL(core_pos), 0x443cU);

	/* Assert CPU_PWRDUP. */
	mmio_write_32(CPU_HEAD_SWITCH_CTL(core_pos), 0x428U);
}

/*
 * qti_pwr_domain_on_finish - per-core setup once it has come online.
 *
 * Initialise the GIC redistributor for the core and request CPUCP to enable
 * the core's clock domain on cold boot.
 */
void qti_pwr_domain_on_finish(int core_pos)
{
	(void)core_pos;

	plat_qti_gic_pcpu_init();
	cpucp_clkdom_init();
}
