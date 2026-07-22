/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Qualcomm Shared Memory (SMEM) - Internal platform abstraction interface
 *
 */

#ifndef QTI_SMEM_PLAT_H
#define QTI_SMEM_PLAT_H

#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <common/debug.h>
#include "qti_smem.h"

/**
 * Log an error-level message.
 */
#define QTI_SMEM_PLAT_LOG_ERR(...) ERROR(__VA_ARGS__)

/**
 * Log a debug-level message.
 */
#define QTI_SMEM_PLAT_LOG_DBG(...) VERBOSE(__VA_ARGS__)

/*
 * QTI_SMEM_PACKED - suppress compiler padding in on-wire structures.
 */
#define QTI_SMEM_PACKED __packed

/* -----------------------------------------------------------------------
 * Mapping attribute flags passed to qti_smem_plat_map().
 * -----------------------------------------------------------------------
 */

/** QTI_SMEM_PLAT_MAP_RO - install a read-only mapping. */
#define QTI_SMEM_PLAT_MAP_RO 0x1U

/** QTI_SMEM_PLAT_MAP_RW - install a read-write mapping. */
#define QTI_SMEM_PLAT_MAP_RW 0x2U

/* -----------------------------------------------------------------------
 * Platform target information
 * -----------------------------------------------------------------------
 */

/**
 * struct qti_smem_plat_info - platform-supplied SMEM target parameters.
 * @local_host: SMEM host ID of the processor running this driver instance.
 *              Must be constructed with qti_smem_host_id().
 * @max_items:  Maximum item index the driver will accept (exclusive upper
 *              bound for the @item argument to qti_smem_lookup()).
 * @smem_size:  Total size of the SMEM region in bytes.
 *
 */
struct qti_smem_plat_info {
	qti_smem_host_t local_host;
	uint16_t max_items;
	size_t smem_size;
};

/* -----------------------------------------------------------------------
 * Platform operation interface
 *
 * Implemented once per platform in qti_smem_plat_xxx.c.
 * The common core calls these; it does not know PA/VA/MMU details.
 * -----------------------------------------------------------------------
 */

/**
 * qti_smem_plat_init() - Platform entry point for SMEM initialization.
 * @plat_info: caller-allocated struct to fill with platform parameters.
 *
 * Discovers SMEM target parameters (PA base, size, max_items, local host)
 * from WONCE registers or device tree and writes them into @plat_info.
 *
 * PA base and VA base are stored in platform-private state only.
 *
 * Return: 0 on success, -EINVAL if @plat_info is NULL, -ENODEV if
 * target info is unavailable, -EIO if target info is corrupted.
 */
int qti_smem_plat_init(struct qti_smem_plat_info *plat_info);

/**
 * qti_smem_plat_map() - Map a region of SMEM into the virtual address space.
 * @offset: byte offset from the SMEM physical base (not PA, not VA).
 * @size:   number of bytes to map.
 * @flags:  QTI_SMEM_PLAT_MAP_RO or QTI_SMEM_PLAT_MAP_RW.
 *
 * The platform converts the offset to physical and virtual addresses:
 *   PA = smem_pa_base + offset
 *   VA = smem_reserved_va_base + offset
 *
 * Must be idempotent: if the range is already mapped with compatible
 * permissions, return 0 without error.
 *
 * Must validate that offset + size does not exceed smem_size.
 *
 * Return: 0 on success, negative errno on failure.
 */
int qti_smem_plat_map(uint32_t offset, size_t size, uint32_t flags);

/**
 * qti_smem_plat_get_addr() - Translate an SMEM offset to a virtual address.
 * @offset: byte offset from the SMEM physical base.
 *
 * Returns the virtual address corresponding to the given offset within
 * the pre-reserved SMEM virtual address window.  Must not perform any
 * new mapping; the region must have been mapped by qti_smem_plat_map()
 * before this function is called.
 *
 * Return: virtual address on success, NULL if the offset is invalid or
 * the region has not been mapped.
 */
void *qti_smem_plat_get_addr(uint32_t offset);

/**
 * qti_smem_plat_mem_barrier() - Full memory barrier.
 *
 * Ensures that all preceding memory accesses are globally visible before
 * any subsequent accesses.  Used before reading mutable shared-memory
 * metadata that may have been written by a remote processor.
 */
static inline void qti_smem_plat_mem_barrier(void)
{
}

/**
 * qti_smem_plat_hwlock_acquire() - Acquire the SMEM serialisation lock.
 *
 * Must serialise concurrent calls to qti_smem_lookup() on the local
 * processor.  Production ports should also acquire the SMEM hardware
 * spinlock to serialise against remote writers that update partition
 * heap pointers when allocating new items.
 *
 * Do NOT hold this lock during MMU mapping operations.
 *
 * Return: 0 on success, negative errno on failure.
 */
static inline int qti_smem_plat_hwlock_acquire(void)
{
	return 0;
}

/**
 * qti_smem_plat_hwlock_release() - Release the SMEM serialisation lock.
 *
 * Must be called on every return path after a successful
 * qti_smem_plat_hwlock_acquire().
 */
static inline void qti_smem_plat_hwlock_release(void)
{
}

#endif /* QTI_SMEM_PLAT_H */
