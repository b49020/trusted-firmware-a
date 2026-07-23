/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * XPU v4 (MPU) static access-control programming. See xpu4.h.
 * Register offsets/fields verified against the downstream HALxPU4HwioGeneric.h
 * and IPCAT (nordschleife_2.0). Programming sequence mirrors xPU4ConfigureRGCfg.
 */

#include <stddef.h>
#include <stdint.h>

#include <arch_helpers.h>
#include <common/debug.h>
#include <lib/mmio.h>
#include <lib/utils_def.h>

#include <nord_def.h>
#include <qti_interrupt_svc.h>
#include <xpu4.h>

/* Global registers (offset from instance base). */
#define XPU4_IDR0		0x000U	/* XPU_TYPE[1:0], NRG[25:16]        */
#define XPU4_IDR1		0x004U	/* CLIENT_ADDR_WIDTH[29:24]         */
#define XPU4_REV		0x00CU	/* MAJOR[31:28], MINOR[27:16]       */
#define XPU4_CFGOWNER		0x404U
#define XPU4_UMRPERMREG		0x408U

/* Error-reporting + syndrome registers (offset from instance base). */
#define XPU4_CLERE		0x20CU	/* client error reporting enable    */
#define XPU4_ESR		0x500U	/* error status: CLERR/CFGERR/multi */
#define XPU4_SYNAR0		0x504U	/* faulting address lo              */
#define XPU4_SYNAR1		0x508U	/* faulting address hi              */

#define XPU4_CLERE_S		BIT(31)	/* report AP-Secure client errors   */
#define XPU4_CLERE_NS		BIT(30)	/* report AP-NonSecure client errors */
#define XPU4_CLERE_QAD0		BIT(0)	/* report QAD0/APPS client errors   */
#define XPU4_ESR_RMSK		0xfU

/* Per region-group registers: base + off + 0x40 * n. */
#define XPU4_RG_STRIDE		0x40U
#define XPU4_RGCR0n		0x1000U	/* RGWOWP[0]                        */
#define XPU4_RGCR1n		0x1004U	/* RGE[0]                           */
#define XPU4_RGCSAR1n		0x1008U	/* start addr hi                    */
#define XPU4_RGCSAR0n		0x100CU	/* start addr lo                    */
#define XPU4_RGCEAR1n		0x1010U	/* end addr hi                      */
#define XPU4_RGCEAR0n		0x1014U	/* end addr lo                      */
#define XPU4_RGRDRn		0x1018U	/* read-perm QAD vector             */
#define XPU4_RGWRRn		0x101CU	/* write-perm QAD vector            */

/* Field masks. */
#define XPU4_IDR0_XPU_TYPE_BMSK		0x3U
#define XPU4_IDR0_XPU_TYPE_MPU		0x0U	/* AC_MPU == 0 */
#define XPU4_IDR0_NRG_BMSK		0x3ff0000U
#define XPU4_IDR0_NRG_SHFT		16U
#define XPU4_IDR1_CLIENT_ADDR_WIDTH_BMSK	0x3f000000U
#define XPU4_IDR1_CLIENT_ADDR_WIDTH_SHFT	24U
#define XPU4_REV_VER_BMSK		0xffff0000U	/* MAJOR|MINOR */
#define XPU4_REV_4_2			((4U << 28) | (2U << 16))
#define XPU4_RGCR1n_RGE			0x1U
#define XPU4_RGCR0n_RGWOWP		0x1U

static uintptr_t xpu4_rg_reg(uintptr_t base, uint32_t off, uint32_t n)
{
	return base + off + (uintptr_t)(XPU4_RG_STRIDE * n);
}

static bool xpu4_is_mpu(uint32_t idr0)
{
	return (idr0 & XPU4_IDR0_XPU_TYPE_BMSK) == XPU4_IDR0_XPU_TYPE_MPU;
}

static bool xpu4_rev_ge_4_2(uint32_t rev)
{
	return (rev & XPU4_REV_VER_BMSK) >= XPU4_REV_4_2;
}

/* Program one region group (mirrors downstream xPU4ConfigureRGCfg). */
static void xpu4_program_rg(uintptr_t base, uint32_t client_addr_width,
			    const struct xpu4_rg *rg)
{
	uint32_t n = rg->rg_num;

	/* Address window: low words always; high words only if >32-bit. */
	mmio_write_32(xpu4_rg_reg(base, XPU4_RGCSAR0n, n),
		      (uint32_t)(rg->start & 0xffffffffU));
	mmio_write_32(xpu4_rg_reg(base, XPU4_RGCEAR0n, n),
		      (uint32_t)(rg->end & 0xffffffffU));

	/* CLIENT_ADDR_WIDTH is one less than the actual width. */
	if (client_addr_width > 31U) {
		mmio_write_32(xpu4_rg_reg(base, XPU4_RGCSAR1n, n),
			      (uint32_t)(rg->start >> 32));
		mmio_write_32(xpu4_rg_reg(base, XPU4_RGCEAR1n, n),
			      (uint32_t)(rg->end >> 32));
	}

	/* Permission QAD vectors. */
	mmio_write_32(xpu4_rg_reg(base, XPU4_RGRDRn, n), rg->read_qads);
	mmio_write_32(xpu4_rg_reg(base, XPU4_RGWRRn, n), rg->write_qads);

	/* Enable the region group. */
	if ((rg->flags & XPU4_RG_ENABLE) != 0U) {
		mmio_write_32(xpu4_rg_reg(base, XPU4_RGCR1n, n),
			      XPU4_RGCR1n_RGE);
	}

	/* Write-once write-protect (do last - locks further writes). */
	if ((rg->flags & XPU4_RG_WOWP) != 0U) {
		mmio_write_32(xpu4_rg_reg(base, XPU4_RGCR0n, n),
			      XPU4_RGCR0n_RGWOWP);
	}
}

static void xpu4_program_instance(const struct xpu4_instance *inst)
{
	uint32_t idr0 = mmio_read_32(inst->base + XPU4_IDR0);
	uint32_t idr1;
	uint32_t rev;
	uint32_t client_addr_width;
	uint32_t hw_nrg;
	uint32_t i;

	if (!xpu4_is_mpu(idr0)) {
		WARN("xpu4: id %u @0x%lx not an MPU (IDR0=0x%x); skip\n",
		     inst->xpu_id, inst->base, idr0);
		return;
	}

	idr1 = mmio_read_32(inst->base + XPU4_IDR1);
	rev = mmio_read_32(inst->base + XPU4_REV);
	client_addr_width = (idr1 & XPU4_IDR1_CLIENT_ADDR_WIDTH_BMSK) >>
			    XPU4_IDR1_CLIENT_ADDR_WIDTH_SHFT;
	hw_nrg = ((idr0 & XPU4_IDR0_NRG_BMSK) >> XPU4_IDR0_NRG_SHFT) + 1U;

	VERBOSE("xpu4: id %u @0x%lx IDR0=0x%x nrg=%u rev=0x%x\n",
		inst->xpu_id, inst->base, idr0, hw_nrg, rev);

	/* Region groups. */
	for (i = 0U; i < inst->nrg; i++) {
		if (inst->rgs[i].rg_num >= hw_nrg) {
			WARN("xpu4: id %u rg %u >= hw nrg %u; skip\n",
			     inst->xpu_id, inst->rgs[i].rg_num, hw_nrg);
			continue;
		}
		xpu4_program_rg(inst->base, client_addr_width, &inst->rgs[i]);
	}

	/* Unmapped-region permission + config owner are rev >= 4.2 features. */
	if (xpu4_rev_ge_4_2(rev)) {
		if ((inst->flags & XPU4_INST_SET_CFGOWNER) != 0U) {
			mmio_write_32(inst->base + XPU4_CFGOWNER,
				      inst->cfg_owner);
		}
		if ((inst->flags & XPU4_INST_SET_UMR) != 0U) {
			uint32_t rb;

			mmio_write_32(inst->base + XPU4_UMRPERMREG,
				      inst->umr_perm);
			rb = mmio_read_32(inst->base + XPU4_UMRPERMREG);
			if (rb != inst->umr_perm) {
				WARN("xpu4: id %u UMRPERM want 0x%x got 0x%x\n",
				     inst->xpu_id, inst->umr_perm, rb);
			}
		}
		if ((inst->flags & XPU4_INST_ERR_REPORT) != 0U) {
			/*
			 * Enable client error reporting so a violation by any
			 * of AP-Secure / AP-NonSecure / QAD0 raises the XPU
			 * malicious-summary IRQ (INTID 0xE3). RMW to preserve
			 * bits XBL may already have set.
			 */
			uint32_t clere = mmio_read_32(inst->base + XPU4_CLERE);

			clere |= (XPU4_CLERE_S | XPU4_CLERE_NS | XPU4_CLERE_QAD0);
			mmio_write_32(inst->base + XPU4_CLERE, clere);
		}
	} else if ((inst->flags &
		    (XPU4_INST_SET_UMR | XPU4_INST_SET_CFGOWNER)) != 0U) {
		WARN("xpu4: id %u rev 0x%x < 4.2; UMR/CFGOWNER skipped\n",
		     inst->xpu_id, rev);
	}
}

void xpu4_apply_static_config(const struct xpu4_instance *insts,
			      uint32_t count)
{
	uint32_t i;

	for (i = 0U; i < count; i++) {
		xpu4_program_instance(&insts[i]);
	}

	dmbsy();
	isb();
}

/*
 * Minimal XPU v4 violation ISR.
 *
 * INTID 0xE3 (xpu4_malicious_summary_irq_apss) is a summary: the TCSR
 * XPU4 malicious-interrupt status registers indicate which instance faulted.
 * This handler logs the raw summary words (each set bit = one XPU instance,
 * decode reg/bit via the downstream bit_mapping table), and for every
 * config'd instance with a pending ESR it logs the ESR + faulting address and
 * clears the ESR. It does NOT do full syndrome (SYNR0-2) decode - that is left
 * to a later cut.
 */
static const struct xpu4_instance *g_xpu4_insts;
static uint32_t g_xpu4_inst_count;

void *xpu4_violation_isr(uint32_t id, void *ctx)
{
	uint32_t reg;
	uint32_t i;
	bool any = false;

	(void)ctx;

	/* Summary status: NORD_XPU4_TCSR_STATUS_BASE + 4*reg, num regs fixed. */
	for (reg = 0U; reg < NORD_XPU4_TCSR_STATUS_NUM; reg++) {
		uint32_t st = mmio_read_32(NORD_XPU4_TCSR_STATUS_BASE +
					   (reg * 4U));
		if (st != 0U) {
			ERROR("xpu4: violation summary reg%u = 0x%08x (INTID 0x%x)\n",
			      reg, st, id);
			any = true;
		}
	}

	/* Per config'd instance: log + clear any pending error status. */
	for (i = 0U; i < g_xpu4_inst_count; i++) {
		uintptr_t base = g_xpu4_insts[i].base;
		uint32_t esr = mmio_read_32(base + XPU4_ESR) & XPU4_ESR_RMSK;

		if (esr != 0U) {
			ERROR("xpu4: id %u ESR=0x%x SYNAR=0x%08x%08x\n",
			      g_xpu4_insts[i].xpu_id, esr,
			      mmio_read_32(base + XPU4_SYNAR1),
			      mmio_read_32(base + XPU4_SYNAR0));
			/* Clear the error (write 0 to ESR). */
			mmio_write_32(base + XPU4_ESR, 0U);
			any = true;
		}
	}

	if (!any) {
		WARN("xpu4: summary IRQ 0x%x with no pending status\n", id);
	}

	dmbsy();
	isb();
	return NULL;
}

int xpu4_register_isr(const struct xpu4_instance *insts, uint32_t count)
{
	g_xpu4_insts = insts;
	g_xpu4_inst_count = count;

	return qti_interrupt_svc_register(NORD_INT_ID_XPU_SEC,
					  xpu4_violation_isr, NULL);
}
