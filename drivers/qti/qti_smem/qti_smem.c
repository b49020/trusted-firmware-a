/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Qualcomm Shared Memory (SMEM) - Common Core
 *
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "qti_smem.h"
#include "qti_smem_plat.h"

/* -----------------------------------------------------------------------
 * Protocol constants
 * -----------------------------------------------------------------------
 */

/*
 * BOOT info page: the first 4096 bytes of SMEM contain BOOT/static
 * version metadata.  This page is mapped read-only during init.
 *
 * Layout of the first 4096 bytes:
 *   [0,   64): proc_comm[16]  - legacy IPC mechanism (16 x uint32_t)
 *   [64, 192): ver[32]        - version array (32 x uint32_t)
 *   [192, 4096): ...          - other static metadata
 *
 * The BOOT SMEM version is at ver[QTI_SMEM_VERSION_BOOT_OFFSET].
 */
#define QTI_SMEM_BOOT_INFO_SIZE 4096U

/** TOC page size: the TOC occupies the last 4096 bytes of SMEM. */
#define QTI_SMEM_TOC_SIZE 4096U

/** Supported TOC version. */
#define QTI_SMEM_TOC_VERSION 1U

/** Maximum number of TOC entries processed (bounds the TOC walk). */
#define QTI_SMEM_TOC_MAX_ENTRIES 40U

/*
 * TOC magic: "$TOC" stored as a little-endian uint32_t.
 *   '$' = 0x24, 'T' = 0x54, 'O' = 0x4F, 'C' = 0x43
 */
#define QTI_SMEM_TOC_MAGIC 0x434F5424U

/*
 * Partition magic: "$PRT" stored as a little-endian uint32_t.
 *   '$' = 0x24, 'P' = 0x50, 'R' = 0x52, 'T' = 0x54
 */
#define QTI_SMEM_PART_MAGIC 0x54525024U

/** Canary value stored in every item header. */
#define QTI_SMEM_ITEM_CANARY 0xa5a5U

/*
 * BOOT SMEM version constants.
 *
 * The BOOT SMEM version word is stored at index QTI_SMEM_VERSION_BOOT_OFFSET
 * within the ver[] array of struct qti_smem_static_header.
 *
 * Version word format:
 *   bits [31:16] = major version
 *   bits [15:0]  = minor version
 *
 * This driver supports major version 0x000C (12).
 * Minor version differences are accepted if the layout is compatible.
 */
#define QTI_SMEM_VERSION_ID 0x000C0001U
#define QTI_SMEM_MAJOR_VERSION_MASK 0xffff0000U
#define QTI_SMEM_MINOR_VERSION_MASK 0x0000ffffU
#define QTI_SMEM_VERSION_BOOT_OFFSET 7U

/*
 * Internal multi-host partition marker.
 * A TOC entry with host0 == host1 == QTI_SMEM_HOST_MULTIHOST describes
 * a multi-host partition whose membership is encoded in hosts_bitmap.
 * Not exposed publicly.
 */
#define QTI_SMEM_HOST_MULTIHOST 0xfffcU

/* -----------------------------------------------------------------------
 * Host-ID encoding
 *
 * A regular host ID is a 16-bit value encoded as:
 *
 *   bits [5:0]   proc_id   (6 bits, values 0-63)
 *   bits [9:6]   proc_num  (4 bits, values 0-15)
 *   bits [12:10] pd_num    (3 bits, values 0-7)
 *   bits [15:13] chiplet   (3 bits, values 0-7)
 *
 * Special values that must never be produced by qti_smem_host_id():
 *   QTI_SMEM_HOST_COMMON    = 0xfffe
 *   QTI_SMEM_HOST_INVALID   = 0xffff
 *   QTI_SMEM_HOST_MULTIHOST = 0xfffc
 * -----------------------------------------------------------------------
 */

#define HOST_PROC_ID_BITS 6U
#define HOST_PROC_NUM_BITS 4U
#define HOST_PD_NUM_BITS 3U
#define HOST_CHIPLET_BITS 3U

#define HOST_PROC_ID_SHIFT 0U
#define HOST_PROC_NUM_SHIFT 6U
#define HOST_PD_NUM_SHIFT 10U
#define HOST_CHIPLET_SHIFT 13U

#define HOST_PROC_ID_MASK ((1U << HOST_PROC_ID_BITS) - 1U)
#define HOST_PROC_NUM_MASK ((1U << HOST_PROC_NUM_BITS) - 1U)
#define HOST_PD_NUM_MASK ((1U << HOST_PD_NUM_BITS) - 1U)
#define HOST_CHIPLET_MASK ((1U << HOST_CHIPLET_BITS) - 1U)

/* -----------------------------------------------------------------------
 * Shared-memory ABI structures
 *
 * These structures represent the fixed binary layout of the SMEM protocol.
 * They are internal to this file and must NOT be exposed in any header.
 * -----------------------------------------------------------------------
 */

/*
 * struct qti_smem_static_header - layout of the first 4096 bytes of SMEM.
 *
 * The ver[] array at offset 64 contains version words for each SMEM
 * subsystem.  Index QTI_SMEM_VERSION_BOOT_OFFSET (7) holds the BOOT
 * SMEM version that this driver validates during init.
 */
struct qti_smem_static_header {
	uint32_t proc_comm[16]; /* legacy IPC: 16 x uint32_t = 64 bytes    */
	uint32_t ver[32]; /* version array: 32 x uint32_t = 128 bytes */
} QTI_SMEM_PACKED;

/*
 * struct qti_smem_toc_header - SMEM partition table (TOC) header.
 *
 * Located at: smem_base + smem_size - QTI_SMEM_TOC_SIZE
 * Immediately followed by an array of qti_smem_toc_entry records.
 */
struct qti_smem_toc_header {
	uint32_t magic; /* must equal QTI_SMEM_TOC_MAGIC              */
	uint32_t version; /* must equal QTI_SMEM_TOC_VERSION            */
	uint32_t num_entries; /* number of valid qti_smem_toc_entry records */
	uint32_t minor_version; /* minor version; informational only           */
	uint32_t reserved[4]; /* reserved; not validated                     */
} QTI_SMEM_PACKED;

/*
 * struct qti_smem_toc_entry - one entry in the SMEM partition table.
 *
 * Entries follow the qti_smem_toc_header immediately in memory.
 * host0 and host1 use uint16_t to match the SMEM wire format.
 * Do NOT use qti_smem_host_t here; the ABI layout must remain stable.
 */
struct qti_smem_toc_entry {
	uint32_t offset; /* byte offset of partition from SMEM base  */
	uint32_t size; /* partition size in bytes                  */
	uint32_t flags; /* reserved flags                           */
	uint16_t host0; /* first host identifier (wire uint16)      */
	uint16_t host1; /* second host identifier (wire uint16)     */
	uint32_t size_cacheline; /* cached-item alignment (0 = default)      */
	uint32_t reserved[3]; /* reserved; not validated                  */
	uint32_t exclusion_sizes[4]; /* per-host exclusion region sizes          */
} QTI_SMEM_PACKED;

/*
 * struct qti_smem_partition_header - header at the start of each partition.
 *
 * host0 and host1 use uint16_t to match the SMEM wire format.
 * Do NOT use qti_smem_host_t here; the ABI layout must remain stable.
 *
 * offset_free_uncached: end of the allocated uncached (upward) region.
 *   Grows upward from sizeof(partition_header).
 * offset_free_cached: start of the allocated cached (downward) region.
 *   Grows downward from partition size.
 */
struct qti_smem_partition_header {
	uint32_t magic; /* must equal QTI_SMEM_PART_MAGIC      */
	uint16_t host0; /* first host identifier (wire uint16)  */
	uint16_t host1; /* second host identifier (wire uint16) */
	uint32_t size; /* total partition size in bytes        */
	uint32_t offset_free_uncached; /* end of allocated uncached region     */
	uint32_t offset_free_cached; /* start of allocated cached region     */
	uint32_t reserved[3]; /* reserved; not validated              */
} QTI_SMEM_PACKED;

/*
 * struct qti_smem_item_header - header preceding each allocated SMEM item.
 *
 * Uncached item layout (growing upward from partition header):
 *   [qti_smem_item_header][padding_header bytes][data][padding_data bytes]
 *
 * Data address:  ptr + sizeof(header) + padding_header
 * Data size:     size - padding_data
 * Next header:   ptr + sizeof(header) + padding_header + size
 */
struct qti_smem_item_header {
	uint16_t canary; /* must equal QTI_SMEM_ITEM_CANARY (0xa5a5) */
	uint16_t item; /* SMEM item identifier                       */
	uint32_t size; /* total rounded size including padding_data  */
	uint16_t padding_data; /* unused bytes at end of data region         */
	uint16_t padding_header; /* alignment gap between header and data      */
	uint32_t reserved; /* reserved; not validated                    */
} QTI_SMEM_PACKED;

/* Compile-time size assertions - catch layout regressions immediately. */
_Static_assert(sizeof(struct qti_smem_static_header) == 192U,
	       "qti_smem_static_header size mismatch");
_Static_assert(sizeof(struct qti_smem_toc_header) == 32U,
	       "qti_smem_toc_header size mismatch");
_Static_assert(sizeof(struct qti_smem_toc_entry) == 48U,
	       "qti_smem_toc_entry size mismatch");
_Static_assert(sizeof(struct qti_smem_partition_header) == 32U,
	       "qti_smem_partition_header size mismatch");
_Static_assert(sizeof(struct qti_smem_item_header) == 16U,
	       "qti_smem_item_header size mismatch");

/* -----------------------------------------------------------------------
 * Driver state
 * -----------------------------------------------------------------------
 */

/*
 * struct qti_smem_info - driver state.
 *
 */
struct qti_smem_info {
	int initialized; /* 0 = uninitialized, 1 = initialized */
	qti_smem_host_t local_host; /* local host ID                      */
	uint16_t max_items; /* maximum item ID (exclusive)        */
	uint32_t smem_size; /* total SMEM size in bytes           */
	uint32_t toc_offset; /* byte offset of TOC from SMEM base  */
	uint32_t num_toc_entries; /* validated TOC entry count          */
	uint32_t common_part_offset; /* common partition offset            */
	uint32_t common_part_size; /* common partition size              */
};

/* Single static instance */
static struct qti_smem_info qti_smem_info;

/* -----------------------------------------------------------------------
 * Internal helpers
 * -----------------------------------------------------------------------
 */

/*
 * smem_rd16() - Read a 16-bit value from shared memory.
 *
 */
static inline uint16_t smem_rd16(const void *ptr)
{
	uint16_t val;

	memcpy(&val, ptr, sizeof(val));
	return val;
}

/*
 * smem_rd32() - Read a 32-bit value from shared memory.
 *
 */
static inline uint32_t smem_rd32(const void *ptr)
{
	uint32_t val;

	memcpy(&val, ptr, sizeof(val));
	return val;
}

/*
 * smem_validate_toc_entry() - Validate a single TOC entry.
 *
 * Checks that the partition described by @e has a non-zero size and lies
 * entirely within the SMEM region.
 *
 * Return: 0 if the entry is valid, -EIO if it is malformed.
 */
static int smem_validate_toc_entry(const struct qti_smem_toc_entry *e)
{
	uint32_t off = smem_rd32(&e->offset);
	uint32_t sz = smem_rd32(&e->size);
	uint64_t end;

	/* Partition must be large enough to hold the partition header. */
	if (sz < (uint32_t)sizeof(struct qti_smem_partition_header))
		return -EIO;
	/* offset + size must not overflow and must lie within SMEM. */
	end = (uint64_t)off + (uint64_t)sz;
	if (end > (uint64_t)qti_smem_info.smem_size)
		return -EIO;
	return 0;
}

/*
 * smem_part_involves_local() - Check if a TOC entry involves the local host.
 *
 * A partition is relevant to the local host if it is:
 *   - the common partition (host0 == host1 == QTI_SMEM_HOST_COMMON), or
 *   - an edge-pair partition where local_host is one of the endpoints.
 *
 * Return: 1 if relevant partition, 0 otherwise.
 */
static int smem_part_involves_local(const struct qti_smem_toc_entry *e)
{
	uint16_t lh = (uint16_t)qti_smem_info.local_host;
	uint16_t h0 = smem_rd16(&e->host0);
	uint16_t h1 = smem_rd16(&e->host1);

	/* Common partition - accessible to all hosts. */
	if ((h0 == (uint16_t)QTI_SMEM_HOST_COMMON) &&
	    (h1 == (uint16_t)QTI_SMEM_HOST_COMMON)) {
		return 1;
	}

	/* Edge-pair partition: local host is one endpoint. */
	return ((h0 == lh) || (h1 == lh)) ? 1 : 0;
}

/*
 * smem_part_matches() - Check if a TOC entry matches the requested host pair.
 *
 *   - If @host == QTI_SMEM_HOST_COMMON: match the common partition.
 *   - Otherwise: match an edge-pair partition where one endpoint is
 *     local_host and the other is @host.
 *
 * Return: 1 if matched partition, 0 otherwise.
 */
static int smem_part_matches(const struct qti_smem_toc_entry *e,
			     qti_smem_host_t host)
{
	uint16_t lh = (uint16_t)qti_smem_info.local_host;
	uint16_t rh = (uint16_t)host;
	uint16_t h0 = smem_rd16(&e->host0);
	uint16_t h1 = smem_rd16(&e->host1);

	/* Common partition lookup. */
	if (host == QTI_SMEM_HOST_COMMON) {
		return ((h0 == (uint16_t)QTI_SMEM_HOST_COMMON) &&
			(h1 == (uint16_t)QTI_SMEM_HOST_COMMON)) ?
			       1 :
			       0;
	}

	/* Edge-pair: one endpoint must be local, the other must be remote. */
	return (((h0 == lh) && (h1 == rh)) || ((h0 == rh) && (h1 == lh))) ? 1 :
									    0;
}

/*
 * smem_scan_uncached() - Scan the uncached/upward item list in a partition.
 *
 * Items are stored sequentially starting immediately after the partition
 * header, growing upward toward @scan_limit.  Each item occupies:
 *
 *   [qti_smem_item_header][padding_header bytes][data (size bytes)]
 *
 * where the last padding_data bytes of data are unused padding.
 *
 * Return:
 *   0       item found; *addr and *size (if non-NULL) are set
 *  -ENOENT  item not present in the uncached region
 *  -EIO     corrupted item metadata detected
 */
static int smem_scan_uncached(const uint8_t *base, uint32_t scan_limit,
			      uint16_t item_id, void **addr, size_t *size)
{
	const uint8_t *limit;
	const uint8_t *ptr;
	const struct qti_smem_item_header *ihdr;
	uint32_t item_size;
	uint32_t step;

	/* scan_limit must cover at least the partition header. */
	if (scan_limit < (uint32_t)sizeof(struct qti_smem_partition_header))
		return -EIO;

	limit = base + scan_limit;
	ptr = base + sizeof(struct qti_smem_partition_header);

	while (ptr < limit) {
		/* Ensure there is room for a complete item header. */
		if ((uintptr_t)limit - (uintptr_t)ptr <
		    sizeof(struct qti_smem_item_header)) {
			return -EIO;
		}

		ihdr = (const struct qti_smem_item_header *)(const void *)ptr;

		/* Validate canary - detects corruption or scan overrun. */
		if (smem_rd16(&ihdr->canary) != (uint16_t)QTI_SMEM_ITEM_CANARY)
			return -EIO;

		item_size = smem_rd32(&ihdr->size);

		/* size must be non-zero; padding must not exceed size. */
		if (item_size == 0U)
			return -EIO;
		if ((uint32_t)smem_rd16(&ihdr->padding_data) > item_size)
			return -EIO;

		/*
		 * step = sizeof(header) + padding_header + item_size.
		 * A wrapped result is always < item_size; detect overflow.
		 */
		step = (uint32_t)sizeof(struct qti_smem_item_header) +
		       (uint32_t)smem_rd16(&ihdr->padding_header) + item_size;
		if (step < item_size)
			return -EIO;
		if ((uintptr_t)limit - (uintptr_t)ptr < (uintptr_t)step)
			return -EIO;

		if (smem_rd16(&ihdr->item) == item_id) {
			/*
			 * Found: payload starts after header + padding_header.
			 * Cast away const: callers may write to the payload.
			 */
			*addr = (void *)(ptr +
					 sizeof(struct qti_smem_item_header) +
					 (size_t)smem_rd16(
						 &ihdr->padding_header));
			if (size != NULL) {
				*size = (size_t)(item_size -
						 (size_t)smem_rd16(
							 &ihdr->padding_data));
			}
			return 0;
		}

		ptr += (size_t)step;
	}

	return -ENOENT;
}

/*
 * smem_search_partition() - Search a single partition for an item.
 *
 * Retrieves the partition virtual address, validates the static header
 * fields (magic, size), issues a memory barrier, acquires the HW lock,
 * reads and validates the mutable heap pointers, then delegates to
 * smem_scan_uncached().
 *
 * The HW lock is released on every return path after a successful acquire.
 *
 * Return:
 *   0       item found
 *  -ENOENT  item not found in uncached region
 *  -EIO     corrupted partition metadata
 *  -EPERM   partition not mapped
 */
static int smem_search_partition(uint32_t part_offset, uint32_t part_size,
				 uint16_t item_id, void **addr, size_t *size)
{
	const struct qti_smem_partition_header *phdr;
	void *va;
	int rc;
	uint32_t offset_free_uncached;
	uint32_t offset_free_cached;
	uint32_t min_uncached;

	/* Validate partition range before any memory access. */
	if (part_size < (uint32_t)sizeof(struct qti_smem_partition_header))
		return -EIO;
	if ((uint64_t)part_offset + (uint64_t)part_size >
	    (uint64_t)qti_smem_info.smem_size) {
		return -EIO;
	}

	va = qti_smem_plat_get_addr(part_offset);
	if (va == NULL)
		return -EPERM;

	phdr = (const struct qti_smem_partition_header *)va;

	/*
	 * Validate static partition fields.
	 * magic and size are written once at partition creation time and
	 * never change, so they can be read without the HW lock.
	 */
	if (smem_rd32(&phdr->magic) != QTI_SMEM_PART_MAGIC) {
		QTI_SMEM_PLAT_LOG_ERR("smem: bad partition magic 0x%08x\n",
				       (unsigned int)smem_rd32(&phdr->magic));
		return -EIO;
	}
	if (smem_rd32(&phdr->size) != part_size) {
		QTI_SMEM_PLAT_LOG_ERR(
			"smem: partition size mismatch (header=%u toc=%u)\n",
			(unsigned int)smem_rd32(&phdr->size),
			(unsigned int)part_size);
		return -EIO;
	}

	/*
	 * Memory barrier before reading mutable partition metadata.
	 * Ensures writes by remote processors are visible on this core.
	 */
	qti_smem_plat_mem_barrier();

	/*
	 * Acquire HW lock before reading mutable heap pointers.
	 * Remote processors update offset_free_uncached and
	 * offset_free_cached when allocating new items.
	 */
	rc = qti_smem_plat_hwlock_acquire();
	if (rc != 0)
		return rc;

	/* Read mutable heap pointers under lock. */
	offset_free_uncached = smem_rd32(&phdr->offset_free_uncached);
	offset_free_cached = smem_rd32(&phdr->offset_free_cached);

	/*
	 * Validate heap pointers:
	 *   offset_free_uncached >= sizeof(partition_header)
	 *   offset_free_uncached <= offset_free_cached
	 *   offset_free_cached   <= part_size
	 */
	min_uncached = (uint32_t)sizeof(struct qti_smem_partition_header);
	if ((offset_free_uncached < min_uncached) ||
	    (offset_free_uncached > offset_free_cached) ||
	    (offset_free_cached > part_size)) {
		qti_smem_plat_hwlock_release();
		QTI_SMEM_PLAT_LOG_ERR(
			"smem: bad heap pointers uncached=%u cached=%u size=%u\n",
			(unsigned int)offset_free_uncached,
			(unsigned int)offset_free_cached,
			(unsigned int)part_size);
		return -EIO;
	}

	rc = smem_scan_uncached((const uint8_t *)va, offset_free_uncached,
				item_id, addr, size);

	/* Release HW lock on every return path after successful acquire. */
	qti_smem_plat_hwlock_release();
	return rc;
}

/*
 * smem_validate_boot_version() - Validate the BOOT SMEM version word.
 *
 * Reads ver[QTI_SMEM_VERSION_BOOT_OFFSET] from the static header and
 * checks that the major version matches QTI_SMEM_VERSION_ID.  A minor
 * version mismatch is tolerated with a warning log.
 *
 * Called from qti_smem_init() after the BOOT info page has been mapped.
 *
 * Return:
 *   0       major version matches (minor mismatch is a warning, not error)
 *  -ENODEV  major version mismatch; SMEM layout is incompatible
 */
static int
smem_validate_boot_version(const struct qti_smem_static_header *static_hdr)
{
	uint32_t boot_version;
	uint32_t boot_major;
	uint32_t local_major;

	boot_version =
		smem_rd32(&static_hdr->ver[QTI_SMEM_VERSION_BOOT_OFFSET]);
	boot_major = boot_version & QTI_SMEM_MAJOR_VERSION_MASK;
	local_major = QTI_SMEM_VERSION_ID & QTI_SMEM_MAJOR_VERSION_MASK;

	if (boot_major != local_major) {
		QTI_SMEM_PLAT_LOG_ERR(
			"smem: BOOT version major mismatch: got 0x%08x expected 0x%08x\n",
			(unsigned int)boot_version, (unsigned int)local_major);
		return -ENODEV;
	}

	return 0;
}

/*
 * smem_validate_toc_header() - Validate the TOC header and entry count.
 *
 * Checks the TOC magic word, version, and num_entries field.  Writes the
 * validated entry count to *num_entries_out on success.
 *
 * Called from qti_smem_init() after the TOC page has been mapped and a
 * memory barrier has been issued.
 *
 * Return:
 *   0       TOC header is valid; *num_entries_out is set
 *  -EIO     bad magic or invalid entry count
 *  -ENODEV  unsupported TOC version
 */
static int smem_validate_toc_header(const struct qti_smem_toc_header *toc,
				    uint32_t *num_entries_out)
{
	uint32_t num_entries;

	if (smem_rd32(&toc->magic) != QTI_SMEM_TOC_MAGIC) {
		QTI_SMEM_PLAT_LOG_ERR("smem: bad TOC magic 0x%08x\n",
				       (unsigned int)smem_rd32(&toc->magic));
		return -EIO;
	}

	if (smem_rd32(&toc->version) != QTI_SMEM_TOC_VERSION) {
		QTI_SMEM_PLAT_LOG_ERR("smem: unsupported TOC version %u\n",
				       (unsigned int)smem_rd32(&toc->version));
		return -ENODEV;
	}

	num_entries = smem_rd32(&toc->num_entries);
	if ((num_entries == 0U) || (num_entries > QTI_SMEM_TOC_MAX_ENTRIES)) {
		QTI_SMEM_PLAT_LOG_ERR("smem: invalid TOC num_entries %u\n",
				       (unsigned int)num_entries);
		return -EIO;
	}

	/* Entries array must fit within the TOC page. */
	if ((uint32_t)sizeof(struct qti_smem_toc_header) +
		    num_entries * (uint32_t)sizeof(struct qti_smem_toc_entry) >
	    QTI_SMEM_TOC_SIZE) {
		QTI_SMEM_PLAT_LOG_ERR("smem: TOC entries overflow TOC page\n");
		return -EIO;
	}

	*num_entries_out = num_entries;
	return 0;
}

/*
 * smem_map_partitions() - Walk the TOC, map local partitions, cache common.
 *
 * Iterates over all @num_entries TOC entries.  For each entry that:
 *   - passes smem_validate_toc_entry() (bounds check), and
 *   - passes smem_part_involves_local() (involves the local host),
 * the partition is mapped read-write via qti_smem_plat_map().
 *
 * Additionally, if the common partition (host0 == host1 ==
 * QTI_SMEM_HOST_COMMON) is found, its offset and size are stored in
 * qti_smem_info.common_part_offset and qti_smem_info.common_part_size
 * so that qti_smem_lookup(QTI_SMEM_HOST_COMMON, ...) can bypass the
 * TOC walk entirely.
 *
 * Unrelated partitions are deliberately not mapped to prevent speculative
 * CPU accesses to regions that may not be accessible from this security
 * domain.
 *
 * Called from qti_smem_init() after qti_smem_info.local_host,
 * qti_smem_info.smem_size, and qti_smem_info.num_toc_entries have been
 * populated.
 *
 * Return:
 *   0       all relevant partitions mapped successfully
 *  non-zero first mapping failure; qti_smem_info is cleared by caller
 */
static int smem_map_partitions(const struct qti_smem_toc_entry *entries,
			       uint32_t num_entries)
{
	uint32_t i;
	int ret;

	for (i = 0U; i < num_entries; i++) {
		const struct qti_smem_toc_entry *e = &entries[i];

		if (smem_validate_toc_entry(e) != 0)
			continue; /* skip malformed entries silently */

		if (smem_part_involves_local(e) == 0)
			continue; /* not relevant partition - do not map */

		ret = qti_smem_plat_map(smem_rd32(&e->offset),
					 (size_t)smem_rd32(&e->size),
					 QTI_SMEM_PLAT_MAP_RW);
		if (ret != 0) {
			QTI_SMEM_PLAT_LOG_ERR(
				"smem: failed to map partition host0=%u host1=%u offset=%u: %d\n",
				(unsigned int)smem_rd16(&e->host0),
				(unsigned int)smem_rd16(&e->host1),
				(unsigned int)smem_rd32(&e->offset), ret);
			return ret;
		}

		if ((smem_rd16(&e->host0) == (uint16_t)QTI_SMEM_HOST_COMMON) &&
		    (smem_rd16(&e->host1) == (uint16_t)QTI_SMEM_HOST_COMMON) &&
		    (qti_smem_info.common_part_offset == 0U)) {
			qti_smem_info.common_part_offset =
				smem_rd32(&e->offset);
			qti_smem_info.common_part_size = smem_rd32(&e->size);
		}
	}

	return 0;
}

/* -----------------------------------------------------------------------
 * Public API
 * -----------------------------------------------------------------------
 */

int qti_smem_host_id(uint16_t proc_id, uint16_t proc_num, uint16_t pd_num,
		      uint16_t chiplet, qti_smem_host_t *host)
{
	uint16_t encoded;

	if (host == NULL)
		return -EINVAL;

	/* Range checks: each field must fit in its allocated bit-width. */
	if ((uint32_t)proc_id > HOST_PROC_ID_MASK)
		return -EINVAL;
	if ((uint32_t)proc_num > HOST_PROC_NUM_MASK)
		return -EINVAL;
	if ((uint32_t)pd_num > HOST_PD_NUM_MASK)
		return -EINVAL;
	if ((uint32_t)chiplet > HOST_CHIPLET_MASK)
		return -EINVAL;

	encoded = (uint16_t)(((uint16_t)proc_id << HOST_PROC_ID_SHIFT) |
			     ((uint16_t)proc_num << HOST_PROC_NUM_SHIFT) |
			     ((uint16_t)pd_num << HOST_PD_NUM_SHIFT) |
			     ((uint16_t)chiplet << HOST_CHIPLET_SHIFT));

	/*
	 * Reject any encoding that collides with a reserved/special host.
	 * These checks protect against accidental construction of reserved
	 * values even when the individual field ranges permit it.
	 */
	if (encoded == (uint16_t)QTI_SMEM_HOST_COMMON)
		return -EINVAL;
	if (encoded == (uint16_t)QTI_SMEM_HOST_INVALID)
		return -EINVAL;
	if (encoded == (uint16_t)QTI_SMEM_HOST_MULTIHOST)
		return -EINVAL;

	*host = (qti_smem_host_t)encoded;
	return 0;
}

/*
 * qti_smem_init() - Initialize the SMEM common core.
 *
 * Performs the following steps:
 *   1. Calls qti_smem_plat_init() to obtain platform target parameters.
 *   2. Validates plat_info fields.
 *   3. Maps the first 4KB BOOT/static metadata page read-only.
 *   4. Calls smem_validate_boot_version() to check the BOOT SMEM version.
 *   5. Maps the TOC page (last 4KB) read-only.
 *   6. Calls smem_validate_toc_header() to check TOC magic/version/count.
 *   7. Calls smem_map_partitions() to map local partitions and cache common.
 *   8. Marks the driver as initialized.
 *
 * Return: 0 on success, -EALREADY if already initialized, or a negative
 * errno value on failure.
 */
int qti_smem_init(void)
{
	struct qti_smem_plat_info plat_info;
	const struct qti_smem_static_header *static_hdr;
	const struct qti_smem_toc_header *toc;
	const struct qti_smem_toc_entry *entries;
	void *va;
	uint32_t smem_size;
	uint32_t toc_offset;
	uint32_t num_entries;
	int ret;

	if (qti_smem_info.initialized != 0)
		return -EALREADY;

	/* Step 1: obtain platform target parameters. */
	ret = qti_smem_plat_init(&plat_info);
	if (ret != 0)
		return ret;

	/* Step 2: validate plat_info fields. */
	if (plat_info.local_host == (qti_smem_host_t)QTI_SMEM_HOST_INVALID) {
		QTI_SMEM_PLAT_LOG_ERR("smem: invalid local_host\n");
		return -EINVAL;
	}

	/*
	 * smem_size must hold at least the BOOT info page and the TOC page
	 * without overlap.  The constant sum cannot overflow size_t.
	 */
	if (plat_info.smem_size <
	    (size_t)(QTI_SMEM_BOOT_INFO_SIZE + QTI_SMEM_TOC_SIZE)) {
		QTI_SMEM_PLAT_LOG_ERR("smem: smem_size too small\n");
		return -EINVAL;
	}

	/* smem_size must fit in uint32_t (offsets are uint32_t). */
	if (plat_info.smem_size > (size_t)UINT32_MAX) {
		QTI_SMEM_PLAT_LOG_ERR("smem: smem_size exceeds uint32_t\n");
		return -EINVAL;
	}

	if (plat_info.max_items == 0U) {
		QTI_SMEM_PLAT_LOG_ERR("smem: max_items is zero\n");
		return -EINVAL;
	}

	smem_size = (uint32_t)plat_info.smem_size;
	toc_offset = smem_size - QTI_SMEM_TOC_SIZE;

	/* Step 3: map the BOOT/static metadata page read-only. */
	ret = qti_smem_plat_map(0U, QTI_SMEM_BOOT_INFO_SIZE,
				 QTI_SMEM_PLAT_MAP_RO);
	if (ret != 0) {
		QTI_SMEM_PLAT_LOG_ERR("smem: failed to map BOOT info: %d\n",
				       ret);
		return ret;
	}

	va = qti_smem_plat_get_addr(0U);
	if (va == NULL) {
		QTI_SMEM_PLAT_LOG_ERR("smem: BOOT info not mapped\n");
		return -EIO;
	}

	static_hdr = (const struct qti_smem_static_header *)va;

	/* Step 4: validate the BOOT SMEM version. */
	ret = smem_validate_boot_version(static_hdr);
	if (ret != 0)
		return ret;

	/* Step 5: map the TOC page read-only. */
	ret = qti_smem_plat_map(toc_offset, QTI_SMEM_TOC_SIZE,
				 QTI_SMEM_PLAT_MAP_RO);
	if (ret != 0) {
		QTI_SMEM_PLAT_LOG_ERR("smem: failed to map TOC: %d\n", ret);
		return ret;
	}

	va = qti_smem_plat_get_addr(toc_offset);
	if (va == NULL) {
		QTI_SMEM_PLAT_LOG_ERR("smem: TOC not mapped\n");
		return -EIO;
	}

	toc = (const struct qti_smem_toc_header *)va;
	entries = (const struct qti_smem_toc_entry
			   *)((const uint8_t *)va +
			      sizeof(struct qti_smem_toc_header));

	/* Memory barrier before reading shared-memory metadata. */
	qti_smem_plat_mem_barrier();

	/* Step 6: validate the TOC header. */
	ret = smem_validate_toc_header(toc, &num_entries);
	if (ret != 0)
		return ret;

	/*
	 * Populate qti_smem_info fields needed by smem_validate_toc_entry(),
	 * smem_part_involves_local(), and smem_map_partitions() before
	 * calling them.
	 */
	qti_smem_info.local_host = plat_info.local_host;
	qti_smem_info.max_items = plat_info.max_items;
	qti_smem_info.smem_size = smem_size;
	qti_smem_info.toc_offset = toc_offset;
	qti_smem_info.num_toc_entries = num_entries;

	/*
	 * Step 7: map local partitions and cache the common partition.
	 *
	 * smem_map_partitions() walks the TOC, maps every partition that
	 * involves the local host, and stores the common partition offset
	 * and size in qti_smem_info for fast O(1) access.
	 */
	ret = smem_map_partitions(entries, num_entries);
	if (ret != 0) {
		/* Roll back partially populated state. */
		memset(&qti_smem_info, 0, sizeof(qti_smem_info));
		return ret;
	}

	/* Step 8: mark driver as initialized. */
	qti_smem_info.initialized = 1;

	return 0;
}

/*
 * qti_smem_lookup() - Look up an existing SMEM item.
 *
 * Fast path (QTI_SMEM_HOST_COMMON):
 *   Uses the cached common_part_offset / common_part_size from
 *   qti_smem_info to call smem_search_partition() directly, bypassing
 *   the TOC walk entirely.
 *
 * Slow path (edge-pair host):
 *   Walks the TOC to find a partition matching the requested host pair,
 *   then calls smem_search_partition() for the first matching entry.
 */
int qti_smem_lookup(qti_smem_host_t remote_host, uint16_t item,
		     uint32_t flags, void **item_ptr, size_t *item_size)
{
	const struct qti_smem_toc_entry *entries;
	void *toc_va;
	uint32_t i;
	int rc;

	/* Validate arguments. */
	if (item_ptr == NULL)
		return -EINVAL;
	if (remote_host == QTI_SMEM_HOST_INVALID)
		return -EINVAL;
	if (flags != 0U)
		return -EINVAL;

	if (qti_smem_info.initialized == 0)
		return -ENODEV;

	if ((uint32_t)item >= (uint32_t)qti_smem_info.max_items)
		return -EINVAL;

	/*
	 * Fast path: common partition lookup.
	 *
	 * The common partition offset and size were cached during
	 * qti_smem_init() by smem_map_partitions().  Use them directly
	 * to avoid walking the TOC on every common-partition lookup.
	 *
	 * If no common partition was found during init (common_part_offset
	 * == 0 and common_part_size == 0), continue below to the TOC walk
	 * which will also return -ENOENT.
	 */
	if (remote_host == QTI_SMEM_HOST_COMMON) {
		if ((qti_smem_info.common_part_offset != 0U) ||
		    (qti_smem_info.common_part_size != 0U)) {
			return smem_search_partition(
				qti_smem_info.common_part_offset,
				qti_smem_info.common_part_size, item, item_ptr,
				item_size);
		}
		/* No common partition mapped - item cannot exist. */
		return -ENOENT;
	}

	/*
	 * Slow path: edge-pair partition lookup.
	 *
	 * Walk the TOC looking for a partition matching {local_host, host}.
	 * First matching valid partition wins.
	 */
	toc_va = qti_smem_plat_get_addr(qti_smem_info.toc_offset);
	if (toc_va == NULL)
		return -EIO;

	entries = (const struct qti_smem_toc_entry
			   *)((const uint8_t *)toc_va +
			      sizeof(struct qti_smem_toc_header));

	/* Memory barrier before reading TOC metadata. */
	qti_smem_plat_mem_barrier();

	for (i = 0U; i < qti_smem_info.num_toc_entries; i++) {
		const struct qti_smem_toc_entry *e = &entries[i];

		if (smem_validate_toc_entry(e) != 0)
			continue;

		if (smem_part_matches(e, remote_host) == 0)
			continue;

		rc = smem_search_partition(smem_rd32(&e->offset),
					   smem_rd32(&e->size), item, item_ptr,
					   item_size);
		if (rc != -ENOENT)
			return rc; /* found, or hard error */
	}

	return -ENOENT;
}
