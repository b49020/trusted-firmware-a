/*
 * Copyright (c) 2026 Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <lib/utils_def.h>

#include <drivers/qti/pdc/pdc_internal.h>

/* Valid GPIO mux input numbers for nord (SA8797P) APSS */
struct pdc_gpio_inputs g_pdc_gpio_inputs[] = {
	/* Input 0 */
	{   0, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_0 */
	{   1, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_1 */
	{   3, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_3 */
	{   4, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_4 */

	/* Input 4 */
	{   6, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_6 */
	{   7, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_7 */
	{   9, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_9 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */

	/* Input 8 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{ 102, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_102 */
	{ 108, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_108 */

	/* Input 12 */
	{ 161, PDC_GPIO_INVALID }, /* aoss_wakeup_gpio_161 */
	{ 114, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_114 */
	{ 116, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_116 */
	{   2, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_2 */

	/* Input 16 */
	{   5, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_5 */
	{   8, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_8 */
	{  11, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_11 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */

	/* Input 20 */
	{ 138, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_138 */
	{ 142, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_142 */
	{ 144, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_144 */
	{ 153, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_153 */

	/* Input 24 */
	{ 157, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_157 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */

	/* Input 28 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{  46, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_46 */

	/* Input 32 */
	{ 126, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_126 */
	{ 128, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_128 */
	{ 132, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_132 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */

	/* Input 36 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */

	/* Input 40 */
	{  45, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_45 */
	{ 124, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_124 */
	{ 166, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_166 */
	{ 160, PDC_GPIO_INVALID }, /* aoss_wakeup_gpio_160 */

	/* Input 44 */
	{ 168, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_168 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */

	/* Input 48 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{ 120, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_120 */
	{ 159, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_159 */

	/* Input 52 */
	{  10, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_10 */
	{ 110, PDC_GPIO_INVALID }, /* core_bi_px_core_in_mx_gpio_110 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */

	/* Input 56 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */

	/* Input 60 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */

	/* Input 64 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */

	/* Input 68 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */

	/* Input 72 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */

	/* Input 76 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */

	/* Input 80 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */

	/* Input 84 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */

	/* Input 88 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */

	/* Input 92 */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
	{   0, PDC_GPIO_INVALID }, /* open (gp_irq_hvm) */
};

const uint32_t g_pdc_gpio_input_size =
	ARRAY_SIZE(g_pdc_gpio_inputs);
