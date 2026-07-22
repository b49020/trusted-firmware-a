#
# Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# SPDX-License-Identifier: BSD-3-Clause
#

PLAT_DRIVERS_PATH :=	drivers/qti

PLAT_INCLUDES	+=	-Iinclude/drivers/qti \
			-Iinclude/drivers/qti/qti_smem

BL31_SOURCES	+=	$(PLAT_DRIVERS_PATH)/qti_smem/qti_smem.c \
			$(PLAT_DRIVERS_PATH)/qti_smem/qti_smem_plat.c
