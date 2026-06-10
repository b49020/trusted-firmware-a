/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * SMEM HW_SW_BUILD_ID (socinfo) layout, populated by boot firmware.
 */

#ifndef PLATFORMINFO_H
#define PLATFORMINFO_H

#include <stdint.h>

/*
 * Generate a platform version number.
 */
#define PLATFORMINFO_VERSION(major, minor) (((major) << 16) | (minor))

/*
 * Version of the platforminfo_smem_t structure. Minor revision ticks
 * are backwards compatible.
 */
#define PLATFORMINFO_SMEM_FORMAT_VERSION(x) x

/*
 * PlatformInfo return values
 */
enum platforminfo_result {
	PLATFORMINFO_SUCCESS = 0,
	PLATFORMINFO_ERROR_NOT_FOUND = -1,
	PLATFORMINFO_ERROR_INVALID_PARAMETER = -2,
	PLATFORMINFO_ERROR_INVALID_DATA = -4,
};

/*
 * Target type of the device on which the platform is running.
 */
enum platforminfo_platform_type {
	PLATFORMINFO_TYPE_UNKNOWN = 0x00,
	PLATFORMINFO_TYPE_SURF = 0x01,
	PLATFORMINFO_TYPE_FFA = 0x02,
	PLATFORMINFO_TYPE_FLUID = 0x03,
	PLATFORMINFO_TYPE_FUSION = 0x04,
	PLATFORMINFO_TYPE_OEM = 0x05,
	PLATFORMINFO_TYPE_QT = 0x06,
	PLATFORMINFO_TYPE_CDP = PLATFORMINFO_TYPE_SURF,
	PLATFORMINFO_TYPE_MTP = 0x08,
	PLATFORMINFO_TYPE_MTP_MDM = PLATFORMINFO_TYPE_MTP,
	PLATFORMINFO_TYPE_MTP_MSM = PLATFORMINFO_TYPE_MTP,
	PLATFORMINFO_TYPE_LIQUID = 0x09,
	PLATFORMINFO_TYPE_DRAGONBOARD = 0x0A,
	PLATFORMINFO_TYPE_QRD = 0x0B,
	PLATFORMINFO_TYPE_EVB = 0x0C,
	PLATFORMINFO_TYPE_HRD = 0x0D,
	PLATFORMINFO_TYPE_DTV = 0x0E,
	PLATFORMINFO_TYPE_RUMI = 0x0F,
	PLATFORMINFO_TYPE_VIRTIO = 0x10,
	PLATFORMINFO_TYPE_GOBI = 0x11,
	PLATFORMINFO_TYPE_CBH = 0x12,
	PLATFORMINFO_TYPE_BTS = 0x13,
	PLATFORMINFO_TYPE_XPM = 0x14,
	PLATFORMINFO_TYPE_RCM = 0x15,
	PLATFORMINFO_TYPE_DMA = 0x16,
	PLATFORMINFO_TYPE_STP = 0x17,
	PLATFORMINFO_TYPE_SBC = 0x18,
	PLATFORMINFO_TYPE_ADP = 0x19,
	PLATFORMINFO_TYPE_CHI = 0x1A,
	PLATFORMINFO_TYPE_SDP = 0x1B,
	PLATFORMINFO_TYPE_RRP = 0x1C,
	PLATFORMINFO_TYPE_CLS = 0x1D,
	PLATFORMINFO_TYPE_TTP = 0x1E,
	PLATFORMINFO_TYPE_HDK = 0x1F,
	PLATFORMINFO_TYPE_IOT = 0x20,
	PLATFORMINFO_TYPE_ATP = 0x21,
	PLATFORMINFO_TYPE_IDP = 0x22,
	PLATFORMINFO_TYPE_AEDK = 0x23,
	PLATFORMINFO_TYPE_WDP = 0x24,
	PLATFORMINFO_TYPE_QAM = 0x25,
	PLATFORMINFO_TYPE_QXR = 0x26,
	PLATFORMINFO_TYPE_X100 = 0x27,
	PLATFORMINFO_TYPE_CRD = 0x28,
	PLATFORMINFO_TYPE_QQVP = 0x29,
	PLATFORMINFO_TYPE_DCP = 0x2A,
	PLATFORMINFO_TYPE_QCB = 0x2B,
	PLATFORMINFO_TYPE_QAR = 0x2C,
	PLATFORMINFO_TYPE_WRD = 0x2D,

	PLATFORMINFO_TYPE_32BITS = 0x7FFFFFFF
};

/*
 * Possible Keys to get the value from the KVP data out of the CDT.
 */
enum platforminfo_key_type {
	PLATFORMINFO_KEY_UNKNOWN = 0x00,
	PLATFORMINFO_KEY_DDR_FREQ = 0x01,
	PLATFORMINFO_KEY_GFX_FREQ = 0x02,
	PLATFORMINFO_KEY_CAMERA_FREQ = 0x03,
	PLATFORMINFO_KEY_FUSION = 0x04,
	PLATFORMINFO_KEY_CUST = 0x05,
	PLATFORMINFO_KEY_NAND_SCRUB = 0x06,
	PLATFORMINFO_KEY_SLT = 0x07,
	PLATFORMINFO_KEY_PMIC = 0x08,
	PLATFORMINFO_NUM_KEYS,

	PLATFORMINFO_KEY_32BITS = 0x7FFFFFFF
};



/*
 * Length of the build ID buffer in platforminfo_smem_t.
 */
#define PLATFORMINFO_SMEM_BUILD_ID_LENGTH 32

/*
 * Length of the chip ID buffer in platforminfo_smem_t
 */
#define PLATFORMINFO_CHIP_NAME_MAX_LENGTH 32

/*
 * Maximum number of PMIC devices in platforminfo_smem_t.
 */
#define PLATFORMINFO_SMEM_MAX_PMIC_DEVICES 3

/*
 * Maximum number of key-value pairs supported.
 * Current platforminfo_key_type_t defines 9 keys (PLATFORMINFO_NUM_KEYS),
 * so 16 provides reasonable headroom for future expansion.
 */
#define PLATFORMINFO_MAX_KVPS 16


/*
 * MEM structure for PMIC information.
 */
struct platforminfo_smem_pmic {
	uint32_t pmic_model; /* PMIC device model type, for Badger matches the revision id subtype */
	uint32_t pmic_version; /* PMIC version, same format as Platform version */
};

/*
 * Structure for the shared memory location which is used to store
 * platform, chip, build, and pmic information.
 */
struct platforminfo_smem {
	uint32_t format; /* Format of the structure. */
	uint32_t chip_id; /* Chip ID. */
	uint32_t chip_version; /* Chip version. */
	char build_id[PLATFORMINFO_SMEM_BUILD_ID_LENGTH]; /* Build ID. */
	uint32_t raw_chip_id; /* Raw chip ID. */
	uint32_t raw_chip_version; /* Raw chip version. */
	enum platforminfo_platform_type platform_type; /* Platform type. */
	uint32_t platform_version; /* Platform version. */
	uint32_t fusion; /* TRUE if Fusion; FALSE otherwise. */
	uint32_t platform_subtype; /* Platform subtype. */
	struct platforminfo_smem_pmic pmic_info[PLATFORMINFO_SMEM_MAX_PMIC_DEVICES];
	/* DEPRECATED. May not contain the full list. Use array at pmic_array_offset instead. */
	uint32_t foundry_id; /* Chip foundry ID. */
	uint32_t chip_serial; /* Chip serial number. */
	uint32_t num_pmics; /* Number of PMICs in array. */
	uint32_t pmic_array_offset; /* Offset to array of platforminfo_smem_pmic */
	uint32_t chip_family; /* Chip family. */
	uint32_t raw_device_family; /* Raw device family. */
	uint32_t raw_device_number; /* Raw device number. */
	uint32_t qfprom_chip_id; /* QFPROM Chip ID   */
	char chip_id_str[PLATFORMINFO_CHIP_NAME_MAX_LENGTH]; /* Chip name */
	uint32_t num_clusters; /* Number of clusters used by chipinfo_get_disabled_cpus API */
	uint32_t cluster_array_offset; /* Offset to uint32 array of disabled CPU clusters */
	uint32_t num_parts; /* Number of parts supported by chipinfo_get_disabled_features API */
	uint32_t disabled_features_array_offset; /* Offset to uint32 array of disabled parts */
	uint32_t modem_supported; /* 0 if not supported, nonzero if supported */
	uint32_t feature_code; /* Feature Code  enum for this device */
	uint32_t p_code; /* PCode for this device */
	uint32_t part_name_map_offset; /* Offset of the part name strings for the current chip */
	uint32_t num_part_name_mappings; /* Number of part name strings. */
	uint32_t oem_variant_id; /* OEM Variant ID */
	uint32_t num_kvps; /* Number of KVPS */
	uint32_t kvps_offset; /* Offset of the KVPS */
	uint32_t num_functional_clusters; /* Number of clusters with >=1 enabled core */
	uint32_t boot_cluster; /* Boot cluster index */
	uint32_t boot_core; /* Boot core index */
	uint32_t raw_package_type; /* Raw package type. */
	uint32_t partial_features_array_offset; /* Offset to uint32 array of partial features */
	uint32_t cpu_cores_array_offset; /* Offset to uint32 array of CPU cores per cluster */
	uint32_t anPartInstancesOffset; /* Offset to array of part-instance structures */
	uint32_t nNumPartInstances; /* Length of the part-instance array */
};

#endif /* PLATFORMINFO_H */
