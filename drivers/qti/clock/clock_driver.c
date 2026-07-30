/*
 * Copyright (c) 2026 Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * TF-A clock driver: clock-group bring-up/teardown and source enables.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <common/debug.h>
#include <drivers/qti/clock/clock.h>
#include <drivers/qti/clock/clock_bsp.h>
#include <drivers/qti/clock/clock_driver.h>
#include <drivers/qti/clock/clock_rail.h>

/*
 * BSP configuration data, provided by the chipset back-end.
 */
extern struct clock_tfa_bsp clock_tfa_bsp_config;

static struct clock_drv_ctxt clock_drv_ctxt = {
	.bsp = &clock_tfa_bsp_config,
};

int clock_enable_source(struct clock_source *source)
{
	if (source == NULL) {
		return -1;
	}

	if (source->source != NULL) {
		if (clock_enable_source(source->source) != 0) {
			return -1;
		}
	}

	/* Only votable PLLs are driven here; no source declares an RPM resource. */
	if (source->ref_count == 0U) {
		clock_hal_enable_source(&source->hw_source);
		if (clock_hal_wait_for_source_on(&source->hw_source) != 0) {
			return -1;
		}
	}
	source->ref_count++;

	return 0;
}

static int clock_enable_clock_group_internal(struct clock_group *group)
{
	struct clock_clk_desc *clock;
	struct clock_power_domain_desc *pd;
	bool timeout = false;

	clock_rail_vote(group);

	if (group->access_clocks != NULL) {
		group->num_access_clocks = 0U;
		for (clock = group->access_clocks; clock->cbcr_addr != 0U;
		     clock++) {
			group->num_access_clocks++;

			if (chipinfo_is_part_disabled(clock->part,
						   clock->part_idx)) {
				continue;
			}

			/* Skip the accessor if TF-A already enabled it, so a retry
			 * after a failed group enable does not lose ownership. */
			if (!clock->tfa_enabled) {
				clock_hal_enable_clock(clock);
			}
			if (clock_hal_wait_for_clock_on(clock) != 0) {
				return -1;
			}
		}
	}

	if (group->power_domains != NULL) {
		group->num_power_domains = 0U;
		for (pd = group->power_domains;
		     (pd->gdscr_addr != 0U) || (pd->vote_reg.addr != 0U);
		     pd++) {
			group->num_power_domains++;

			if (chipinfo_is_part_disabled(pd->part, pd->part_idx)) {
				continue;
			}

			if (!pd->tfa_enabled) {
				clock_hal_enable_power_domain(pd);
			}

			/* Poll immediately: some chipsets have sibling GDSCs
			 * (e.g. GPU) that must see this one fully up before
			 * their own enable is issued. */
			if (clock_hal_wait_for_power_domain_on(pd) != 0) {
				return -1;
			}
		}
	}

	group->num_clocks = 0U;
	for (clock = group->clocks; clock->cbcr_addr != 0U; clock++) {
		group->num_clocks++;

		if (chipinfo_is_part_disabled(clock->part, clock->part_idx)) {
			continue;
		}

		if (!clock->tfa_enabled) {
			clock_hal_enable_clock(clock);
		}
	}
	for (clock = group->clocks; clock->cbcr_addr != 0U; clock++) {
		if (chipinfo_is_part_disabled(clock->part, clock->part_idx)) {
			continue;
		}

		timeout |= (clock_hal_wait_for_clock_on(clock) != 0);
	}

	if (timeout) {
		return -1;
	}

	return 0;
}

static int clock_disable_clock_group_internal(struct clock_group *group)
{
	struct clock_clk_desc *clock;
	struct clock_power_domain_desc *pd;
	uint32_t i;

	for (i = group->num_clocks; i > 0U; i--) {
		clock = &group->clocks[i - 1U];
		/* Disable the resource only if TF-A enabled it. */
		if (clock->tfa_enabled) {
			clock_hal_disable_clock(clock);
		}
	}

	if (group->power_domains != NULL) {
		for (i = group->num_power_domains; i > 0U; i--) {
			pd = &group->power_domains[i - 1U];
			if (pd->tfa_enabled) {
				clock_hal_disable_power_domain(pd);
				/* Confirm the GDSC is off before the rail
				 * vote backing it is cleared below. */
				clock_hal_wait_for_power_domain_off(pd);
			}
		}
	}

	if (group->access_clocks != NULL) {
		for (i = group->num_access_clocks; i > 0U; i--) {
			clock = &group->access_clocks[i - 1U];
			if (clock->tfa_enabled) {
				clock_hal_disable_clock(clock);
			}
		}
	}

	clock_rail_clear(group);

	return 0;
}

void qti_clock_init(void)
{
	int ret;

	if (clock_drv_ctxt.initialized) {
		return;
	}

	clock_rail_init();

	ret = clock_init_image(&clock_drv_ctxt);
	if (ret != 0) {
		ERROR("Clock: init failed (%d)\n", ret);
		return;
	}

	clock_drv_ctxt.initialized = true;
}

void qti_clock_init_done(void)
{
	int ret;

	if (!clock_drv_ctxt.initialized) {
		return;
	}

	ret = clock_post_init_image(&clock_drv_ctxt);
	if (ret != 0) {
		ERROR("Clock: init done failed (%d)\n", ret);
	}

	/* Release the driver-lifetime cx/mx rail holds taken at init. */
	clock_rail_deinit();
}

int clock_enable_clock_group(enum clock_group_type group_type)
{
	struct clock_group *group;

	if ((group_type >= CLOCK_GROUP_TOTAL) ||
	    (clock_drv_ctxt.bsp->clock_groups == NULL)) {
		return -1;
	}

	group = &clock_drv_ctxt.bsp->clock_groups[group_type];
	if (group->clocks == NULL) {
		return -1;
	}

	if (group->ref_count++ == 0U) {
		if (clock_enable_clock_group_internal(group) != 0) {
			/* Roll back the vote so a retry re-runs bring-up. */
			group->ref_count--;
			return -1;
		}
	}

	return 0;
}

int clock_disable_clock_group(enum clock_group_type group_type)
{
	struct clock_group *group;

	if ((group_type >= CLOCK_GROUP_TOTAL) ||
	    (clock_drv_ctxt.bsp->clock_groups == NULL)) {
		return -1;
	}

	group = &clock_drv_ctxt.bsp->clock_groups[group_type];
	if (group->clocks == NULL) {
		return -1;
	}

	if ((group->ref_count > 0U) && (group->ref_count-- == 1U)) {
		if (clock_disable_clock_group_internal(group) != 0) {
			return -1;
		}
	}

	return 0;
}
