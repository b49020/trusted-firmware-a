/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Rail voltage voting for the QTI clock driver.
 *
 * Boot-time votes that keep the rails feeding the blocks TF-A programs during
 * init (e.g. CX/MX and MMCX/GFX at NOM for xPU/secure static configuration) up
 * while that programming runs, released at init-done. This is not ongoing
 * voltage scaling (e.g. QUP), which is handled by OP-TEE. RPM-based targets
 * place no such votes here and build the no-op stubs below
 * (QTI_CLOCK_RAIL_VOTE unset).
 */

#ifndef CLOCK_RAIL_H
#define CLOCK_RAIL_H

#include <drivers/qti/clock/clock_bsp.h>

#ifdef QTI_CLOCK_RAIL_VOTE
void clock_rail_init(void);
void clock_rail_deinit(void);
void clock_rail_vote(struct clock_group *group);
void clock_rail_clear(struct clock_group *group);
#else
static inline void clock_rail_init(void) { }
static inline void clock_rail_deinit(void) { }
static inline void clock_rail_vote(struct clock_group *group) { (void)group; }
static inline void clock_rail_clear(struct clock_group *group) { (void)group; }
#endif

#endif /* CLOCK_RAIL_H */
