/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Nord DRAM-based XPU v4 access-control policy discovery ("Option A").
 * See ac_config_parser.c for the full picture.
 */

#ifndef AC_CONFIG_PARSER_H
#define AC_CONFIG_PARSER_H

#include <stdint.h>

struct xpu4_instance;

/*
 * Look up and parse the AC-config DRAM blob via SMEM_NORD_AC_CONFIG_ADDR.
 * Returns a pointer to a translated struct xpu4_instance table and sets
 * *count on success. Returns NULL and sets *count = 0 if the SMEM item is
 * absent, or the blob fails any structural validation - callers should
 * fall back to a compiled-in policy in that case.
 */
const struct xpu4_instance *ac_config_lookup_xpu_cfg(uint32_t *count);

#endif /* AC_CONFIG_PARSER_H */
