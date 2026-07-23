/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Nord access-control init: program the XPU v4 MPUs from the compiled-in
 * static policy (see xpu/nord/xpu_config.c). The dynamic VM memory-assign
 * SMC path is not implemented on Nord yet and remains a no-op.
 */

#include <stdint.h>

#include <common/debug.h>

#include <drivers/qti/accesscontrol/accesscontrol.h>

#include <xpu4.h>

void qti_accesscontrol_init(void)
{
	const struct xpu4_instance *cfg;
	uint32_t count = 0U;

	cfg = nord_xpu_get_config(&count);
	if ((cfg == NULL) || (count == 0U)) {
		WARN("access-control: no Nord XPU config; skipping\n");
		return;
	}

	xpu4_apply_static_config(cfg, count);
	INFO("access-control: programmed %u XPU4 instance(s)\n", count);
}

uint64_t qti_accesscontrol_mem_assign(const qti_accesscontrol_mem_t *mem_info,
				      uint32_t mem_len,
				      const uint32_t *src, uint32_t src_len,
				      const qti_accesscontrol_perm_t *dst,
				      uint32_t dst_len)
{
	/* Dynamic VM memory reassignment not supported on Nord. */
	(void)mem_info;
	(void)mem_len;
	(void)src;
	(void)src_len;
	(void)dst;
	(void)dst_len;

	return 0U;
}
