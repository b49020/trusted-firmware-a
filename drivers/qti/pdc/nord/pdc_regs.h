/*
 * Copyright (c) 2026 Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * PDC register definitions for Nord (SA8797P) APSS.
 */

#ifndef PDC_REGS_H
#define PDC_REGS_H

#include <lib/mmio.h>

#include <platform_def.h>

/*
 * Base addresses. PDC_SEQ_MEM_BASE (and the BR_ADDR/DELAY_VAL registers
 * below) are NOT at the lemans-style QTI_AOSS_BASE+0x400000 GRP0 arena -
 * IPCAT (chip 781, RPMH_PDC_GRP0_SEQ_MEM_m/SEQ_CFG_BR_ADDR_b/
 * SEQ_CFG_DELAY_VAL_v) places all three in the same 0x3F9xxx arena as the
 * TCS command registers below, relative to PDC_BASE.
 */
#define PDC_BASE		(QTI_AOSS_BASE + 0x200000U)
#define PDC_SEQ_MEM_BASE	(PDC_BASE + 0x3F9000U)

/* Register address helpers */
#define PDC_REG(base, off)		((uintptr_t)(base) + (uint32_t)(off))
#define PDC_REG_DRV(base, off, d)	((uintptr_t)(base) + (uint32_t)(off) + \
					 0x10000U * (uint32_t)(d))

/* --------------------------------------------------------------------------
 * ENABLE_PDC  (base + 0x4500)
 * --------------------------------------------------------------------------
 */
/* Nord: ENABLE_PDC is in PDC_GLOBAL (AOSS+0x5D0000)+0x20 = wrapper+0x3D0020 */
#define PDC_ENABLE_PDC_OFF		0x3D0020U
#define PDC_ENABLE_PDC_BMSK		0x1U

#define PDC_ENABLE_PDC_RMW(base, val) \
	mmio_write_32(PDC_REG(base, PDC_ENABLE_PDC_OFF), \
		      (mmio_read_32(PDC_REG(base, PDC_ENABLE_PDC_OFF)) & \
		       ~PDC_ENABLE_PDC_BMSK) | \
		      ((uint32_t)(val) & PDC_ENABLE_PDC_BMSK))

/* --------------------------------------------------------------------------
 * PDC_PARAM_RESOURCE  (base + 0x1004 + 0x10000*d)
 * --------------------------------------------------------------------------
 */
#define PDC_PARAM_RESOURCE_OFF			0x1004U
#define PDC_PARAM_RESOURCE_PROFILING_UNIT_BMSK	0xf000U
#define PDC_PARAM_RESOURCE_PROFILING_UNIT_SHFT	12U
#define PDC_PARAM_RESOURCE_TCS_BMSK		0xf00U
#define PDC_PARAM_RESOURCE_TCS_SHFT		8U
#define PDC_PARAM_RESOURCE_TCS_CMDS_BMSK	0xe0U
#define PDC_PARAM_RESOURCE_TCS_CMDS_SHFT	5U

#define PDC_PARAM_RESOURCE_READ(base, d) \
	mmio_read_32(PDC_REG_DRV(base, PDC_PARAM_RESOURCE_OFF, d))

#define PDC_PARAM_PROFILING_UNIT(base) \
	((PDC_PARAM_RESOURCE_READ(base, 0) & \
	  PDC_PARAM_RESOURCE_PROFILING_UNIT_BMSK) >> \
	 PDC_PARAM_RESOURCE_PROFILING_UNIT_SHFT)

#define PDC_PARAM_TCS_COUNT(base) \
	((PDC_PARAM_RESOURCE_READ(base, 0) & PDC_PARAM_RESOURCE_TCS_BMSK) >> \
	 PDC_PARAM_RESOURCE_TCS_SHFT)

#define PDC_PARAM_TCS_CMDS(base) \
	((PDC_PARAM_RESOURCE_READ(base, 0) & PDC_PARAM_RESOURCE_TCS_CMDS_BMSK) >> \
	 PDC_PARAM_RESOURCE_TCS_CMDS_SHFT)

/* --------------------------------------------------------------------------
 * PDC_PARAM_SEQ_CONFIG  (base + 0x1008 + 0x10000*d)
 * --------------------------------------------------------------------------
 */
#define PDC_PARAM_SEQ_CONFIG_OFF	0x1008U
#define PDC_PARAM_SEQ_CMD_WORDS_BMSK	0xff0000U
#define PDC_PARAM_SEQ_CMD_WORDS_SHFT	16U

#define PDC_PARAM_SEQ_CMD_WORDS(base) \
	((mmio_read_32(PDC_REG_DRV(base, PDC_PARAM_SEQ_CONFIG_OFF, 0)) & \
	  PDC_PARAM_SEQ_CMD_WORDS_BMSK) >> PDC_PARAM_SEQ_CMD_WORDS_SHFT)

/* --------------------------------------------------------------------------
 * SEQ_CFG_BR_ADDR  (IPCAT: RPMH_PDC_GRP0_SEQ_CFG_BR_ADDR_b =
 * PDC_BASE + 0x3F9600 + 0x4*b - NOT base+0x4560 (that offset is lemans-only;
 * on Nord it falls in unmapped/undefined AOSS PDC register space, per IPCAT).
 * Currently dead on Nord: apps_pdc_cfg.br_count == 0 (see pdc_seq_cfg.c), so
 * this write never executes - fixed here anyway to keep the driver generic
 * and correct if a future target/config re-enables static branch addresses.
 * --------------------------------------------------------------------------
 */
#define PDC_SEQ_BR_ADDR_OFF(b)		(0x3F9600U + 0x4U * (uint32_t)(b))

#define PDC_SEQ_BR_ADDR_WRITE(base, b, val) \
	mmio_write_32(PDC_REG(base, PDC_SEQ_BR_ADDR_OFF(b)), (uint32_t)(val))

/* --------------------------------------------------------------------------
 * SEQ_CFG_DELAY_VAL  (IPCAT: RPMH_PDC_GRP0_SEQ_CFG_DELAY_VAL_v =
 * PDC_BASE + 0x3F9700 + 0x4*v - NOT base+0x45A0 (lemans-only offset, same
 * unmapped-on-Nord issue as SEQ_CFG_BR_ADDR above). Also currently dead on
 * Nord (delay_count == 0); fixed for the same reason.
 * --------------------------------------------------------------------------
 */
#define PDC_SEQ_DELAY_VAL_OFF(v)	(0x3F9700U + 0x4U * (uint32_t)(v))

#define PDC_SEQ_DELAY_WRITE(base, v, val) \
	mmio_write_32(PDC_REG(base, PDC_SEQ_DELAY_VAL_OFF(v)), (uint32_t)(val))

/* --------------------------------------------------------------------------
 * SEQ_MEM  (IPCAT: RPMH_PDC_GRP0_SEQ_MEM_m = PDC_SEQ_MEM_BASE + 0x4*m, where
 * PDC_SEQ_MEM_BASE is PDC_BASE+0x3F9000, defined above - NOT
 * QTI_AOSS_BASE+0x400000 as the lemans-copied version had it. This register
 * is currently unreachable in practice: Nord's pdc_seq_cfg.c now matches the
 * real downstream config (all modes cmds=NULL/length=0, AOP-programmed), so
 * pdc_seq_copy_cmd_seq() skips every mode and this RMW is never called - but
 * the offset is fixed here too so it is correct if that ever changes.
 * --------------------------------------------------------------------------
 */
#define PDC_SEQ_MEM_OFF(m)		(0x4U * (uint32_t)(m))

#define PDC_SEQ_MEM_RMW(m, mask, val) \
	mmio_write_32(PDC_REG(PDC_SEQ_MEM_BASE, PDC_SEQ_MEM_OFF(m)), \
		      (mmio_read_32(PDC_REG(PDC_SEQ_MEM_BASE, \
					    PDC_SEQ_MEM_OFF(m))) & \
		       ~(uint32_t)(mask)) | \
		      ((uint32_t)(val) & (uint32_t)(mask)))

/* --------------------------------------------------------------------------
 * TCS_CMD_ENABLE_BANK  (base + 0x5504 + 0xC8*t)
 * --------------------------------------------------------------------------
 */
#define PDC_TCS_CMD_ENABLE_OFF(t)	(0x5000U + 0xc8U * (uint32_t)(t))

#define PDC_TCS_CMD_ENABLE_WRITE(base, t, val) \
	mmio_write_32(PDC_REG(base, PDC_TCS_CMD_ENABLE_OFF(t)), (uint32_t)(val))

/* --------------------------------------------------------------------------
 * TCS_CMD_WAIT_FOR_CMPL_BANK  (base + 0x5508 + 0xC8*t)
 * --------------------------------------------------------------------------
 */
#define PDC_TCS_WAIT_CMPL_OFF(t)	(0x3F9808U + 0xc8U * (uint32_t)(t))

#define PDC_TCS_WAIT_CMPL_READ(base, t) \
	mmio_read_32(PDC_REG(base, PDC_TCS_WAIT_CMPL_OFF(t)))

#define PDC_TCS_WAIT_CMPL_WRITE(base, t, val) \
	mmio_write_32(PDC_REG(base, PDC_TCS_WAIT_CMPL_OFF(t)), (uint32_t)(val))

/* --------------------------------------------------------------------------
 * TCSt_CMDn registers  (base + offset + 0xC8*t + 0x10*n)
 * --------------------------------------------------------------------------
 */
#define PDC_TCS_MSGID_OFF(t, n)	(0x3F980CU + 0xc8U * (uint32_t)(t) + \
				 0x10U * (uint32_t)(n))
#define PDC_TCS_ADDR_OFF(t, n)	(0x3F9810U + 0xc8U * (uint32_t)(t) + \
				 0x10U * (uint32_t)(n))
#define PDC_TCS_DATA_OFF(t, n)	(0x3F9814U + 0xc8U * (uint32_t)(t) + \
				 0x10U * (uint32_t)(n))

#define PDC_TCS_MSGID_WRITE_BMSK	0x10000U	/* READ_OR_WRITE */
#define PDC_TCS_MSGID_RES_REQ_BMSK	0x100U

#define PDC_TCS_MSGID_WRITE(base, t, n, val) \
	mmio_write_32(PDC_REG(base, PDC_TCS_MSGID_OFF(t, n)), (uint32_t)(val))

#define PDC_TCS_ADDR_WRITE(base, t, n, val) \
	mmio_write_32(PDC_REG(base, PDC_TCS_ADDR_OFF(t, n)), (uint32_t)(val))

#define PDC_TCS_DATA_WRITE(base, t, n, val) \
	mmio_write_32(PDC_REG(base, PDC_TCS_DATA_OFF(t, n)), (uint32_t)(val))

/* --------------------------------------------------------------------------
 * IRQ ownership - Nord uses the BANKED model, not a per-bit IRQ_i_OWNER
 * register (IPCAT: RPMH_PDC_APPS_IRQ_i_OWNER does not exist for chip 781;
 * only RPMH_PDC_APPS_IRQ_OWNER_BANKb/_LASTBANK do). Confirmed against the
 * downstream TZ PDC HAL (core/power/pdc/interrupt/hal/src/pdcHal.c,
 * pdcHAL_setOwner()): each bank register is a 32-bit bitmask, one bit per
 * PDC bit number (bank = bitnum/32, bit = 1<<(bitnum%32)) - it does NOT carry
 * a 3-bit DRV field per bit. The actual non-zero-owner DRV number is a
 * single global value in IRQ_NONZERO_DRV_CFG, set once when the first
 * non-default-owned bit is configured (all owned bits on this target must
 * share the same non-zero DRV - the downstream HAL asserts on any second,
 * different DRV value). BANKb covers bit numbers 0..(5*32-1); anything at or
 * beyond bit 160 uses LASTBANK instead (IPCAT: BANKb b=0..4, then a separate
 * LASTBANK register - there is no BANK5).
 *
 * The previous per-bit IRQ_i_OWNER-style macro (copied from lemans, which
 * genuinely has that legacy register) silently wrote into unmapped/undefined
 * AOSS PDC register space for every interrupt/GPIO bit on Nord - up to
 * PDC_BASE+0x4930 for the ~202 static-IRQ + GPIO bits this driver configures,
 * well past the real banked registers at PDC_BASE+0x4600..0x4614+0x4640.
 * --------------------------------------------------------------------------
 */
#define PDC_IRQ_OWNER_BANK_OFF(b)	(0x4600U + 0x4U * (uint32_t)(b))
#define PDC_IRQ_OWNER_LASTBANK_OFF	0x4614U
#define PDC_IRQ_NONZERO_DRV_CFG_OFF	0x4640U
#define PDC_IRQ_OWNER_BANK_MAXb		4U
#define PDC_IRQ_OWNER_BITS_PER_BANK	32U

#define PDC_IRQ_OWNER_WRITE(i, val) pdc_irq_owner_write(i, val)

static inline void pdc_irq_owner_write(uint32_t bit_num, uint32_t owner)
{
	uint32_t bank = bit_num / PDC_IRQ_OWNER_BITS_PER_BANK;
	uint32_t mask = 1U << (bit_num % PDC_IRQ_OWNER_BITS_PER_BANK);
	uintptr_t bank_reg = (bank <= PDC_IRQ_OWNER_BANK_MAXb) ?
		PDC_REG(PDC_BASE, PDC_IRQ_OWNER_BANK_OFF(bank)) :
		PDC_REG(PDC_BASE, PDC_IRQ_OWNER_LASTBANK_OFF);
	uint32_t cur = mmio_read_32(bank_reg);

	if (owner != 0U) {
		mmio_write_32(bank_reg, mask | cur);
		mmio_write_32(PDC_REG(PDC_BASE, PDC_IRQ_NONZERO_DRV_CFG_OFF),
			      owner);
	} else {
		mmio_write_32(bank_reg, (~mask) & cur);
	}
}

#endif /* PDC_REGS_H */
