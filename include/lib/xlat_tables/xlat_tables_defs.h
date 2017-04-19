/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __XLAT_TABLES_DEFS_H__
#define __XLAT_TABLES_DEFS_H__

#include <utils_def.h>

/* Miscellaneous MMU related constants */
#define NUM_2MB_IN_GB		(1 << 9)
#define NUM_4K_IN_2MB		(1 << 9)
#define NUM_GB_IN_4GB		(1 << 2)

#define TWO_MB_SHIFT		21
#define ONE_GB_SHIFT		30
#define FOUR_KB_SHIFT		12

#define ONE_GB_INDEX(x)		((x) >> ONE_GB_SHIFT)
#define TWO_MB_INDEX(x)		((x) >> TWO_MB_SHIFT)
#define FOUR_KB_INDEX(x)	((x) >> FOUR_KB_SHIFT)

/*
 * Terminology:
 *
 * - A block descriptor points to a region of memory bigger than the granule
 *   size (e.g. a 2MB region when the granule size is 4KB).
 *
 * - A page descriptor points to a page, i.e. a memory region whose size is
 *   the translation granule size (e.g. 4KB).
 *
 * - A table descriptor points to the next level of translation table.
 */
#define INVALID_DESC		0x0
#define BLOCK_DESC		0x1 /* Table levels 0-2 */
#define TABLE_DESC		0x3 /* Table levels 0-2 */
#define PAGE_DESC		0x3 /* Table level 3 */
#define DESC_MASK		0x3

#define FIRST_LEVEL_DESC_N	ONE_GB_SHIFT
#define SECOND_LEVEL_DESC_N	TWO_MB_SHIFT
#define THIRD_LEVEL_DESC_N	FOUR_KB_SHIFT

#define XN_SHIFT		54

/*
 * The following definitions must all be passed to the UPPER_ATTRS() macro to
 * get the right bitmask.
 */
/* XN: Translation regimes that support one VA range (EL2 and EL3). */
#define XN			(ULL(1) << 2)
/* UXN, PXN: Translation regimes that support two VA ranges (EL1&0). */
#define UXN			(ULL(1) << 2)
#define PXN			(ULL(1) << 1)
#define CONT_HINT		(ULL(1) << 0)
#define NON_GLOBAL		(1 << 9)
#define ACCESS_FLAG		(1 << 8)
#define NSH			(0x0 << 6)
#define OSH			(0x2 << 6)
#define ISH			(0x3 << 6)
#define UPPER_ATTRS(x)		(((x) & ULL(0x7)) << 52)

#define TABLE_ADDR_MASK		ULL(0x0000FFFFFFFFF000)

/* XXX: Check redundancy with TABLE_ADDR_MASK */
#define XLAT_TABLE_IDX_MASK	0x1ff

#define PAGE_SIZE_SHIFT		FOUR_KB_SHIFT /* 4, 16 or 64 KB */
#define PAGE_SIZE		(1 << PAGE_SIZE_SHIFT)
#define PAGE_SIZE_MASK		(PAGE_SIZE - 1)
#define IS_PAGE_ALIGNED(addr)	(((addr) & PAGE_SIZE_MASK) == 0)

#define XLAT_ENTRY_SIZE_SHIFT	3 /* Each MMU table entry is 8 bytes (1 << 3) */
#define XLAT_ENTRY_SIZE		(1 << XLAT_ENTRY_SIZE_SHIFT)

#define XLAT_TABLE_SIZE_SHIFT	PAGE_SIZE_SHIFT /* Size of one complete table */
#define XLAT_TABLE_SIZE		(1 << XLAT_TABLE_SIZE_SHIFT)

#ifdef AARCH32
#define XLAT_TABLE_LEVEL_MIN	1
#else
#define XLAT_TABLE_LEVEL_MIN	0
#endif /* AARCH32 */

#define XLAT_TABLE_LEVEL_MAX	3

/* Values for number of entries in each MMU translation table */
#define XLAT_TABLE_ENTRIES_SHIFT (XLAT_TABLE_SIZE_SHIFT - XLAT_ENTRY_SIZE_SHIFT)
#define XLAT_TABLE_ENTRIES	(1 << XLAT_TABLE_ENTRIES_SHIFT)
#define XLAT_TABLE_ENTRIES_MASK	(XLAT_TABLE_ENTRIES - 1)

/* Values to convert a memory address to an index into a translation table */
#define L3_XLAT_ADDRESS_SHIFT	PAGE_SIZE_SHIFT
#define L2_XLAT_ADDRESS_SHIFT	(L3_XLAT_ADDRESS_SHIFT + XLAT_TABLE_ENTRIES_SHIFT)
#define L1_XLAT_ADDRESS_SHIFT	(L2_XLAT_ADDRESS_SHIFT + XLAT_TABLE_ENTRIES_SHIFT)
#define L0_XLAT_ADDRESS_SHIFT	(L1_XLAT_ADDRESS_SHIFT + XLAT_TABLE_ENTRIES_SHIFT)
#define XLAT_ADDR_SHIFT(level)	(PAGE_SIZE_SHIFT + \
		  ((XLAT_TABLE_LEVEL_MAX - (level)) * XLAT_TABLE_ENTRIES_SHIFT))

#define XLAT_BLOCK_SIZE(level)	((u_register_t)1 << XLAT_ADDR_SHIFT(level))
/* Mask to get the bits used to index inside a block of a certain level */
#define XLAT_BLOCK_MASK(level)	(XLAT_BLOCK_SIZE(level) - 1)
/* Mask to get the address bits common to a block of a certain table level*/
#define XLAT_ADDR_MASK(level)	(~XLAT_BLOCK_MASK(level))

/*
 * The ARMv8 translation table descriptor format defines AP[2:1] as the Access
 * Permissions bits, and does not define an AP[0] bit.
 *
 * AP[1] is valid only for a stage 1 translation that supports two VA ranges
 * (i.e. in the ARMv8A.0 architecture, that is the S-EL1&0 regime).
 *
 * AP[1] is RES0 for stage 1 translations that support only one VA range
 * (e.g. EL3)
 */
#define AP2_SHIFT			7
#define AP2_RO				1
#define AP2_RW				0

#define AP1_SHIFT			6
#define AP1_ACCESS			1
#define AP1_NO_ACCESS			0
/*
 * The following definitions must all be passed to the LOWER_ATTRS() macro to
 * get the right bitmask.
 */
#define AP_RO				(AP2_RO << 5)
#define AP_RW				(AP2_RW << 5)
#define NS				(0x1 << 3)
#define ATTR_NON_CACHEABLE_INDEX	0x2
#define ATTR_DEVICE_INDEX		0x1
#define ATTR_IWBWA_OWBWA_NTR_INDEX	0x0
#define LOWER_ATTRS(x)			(((x) & 0xfff) << 2)

/* Normal Memory, Outer Write-Through non-transient, Inner Non-cacheable */
#define ATTR_NON_CACHEABLE		(0x44)
/* Device-nGnRE */
#define ATTR_DEVICE			(0x4)
/* Normal Memory, Outer Write-Back non-transient, Inner Write-Back non-transient */
#define ATTR_IWBWA_OWBWA_NTR		(0xff)
#define MAIR_ATTR_SET(attr, index)	((attr) << ((index) << 3))
#define ATTR_INDEX_MASK			0x3
#define ATTR_INDEX_GET(attr)		(((attr) >> 2) & ATTR_INDEX_MASK)

/*
 * Flags to override default values used to program system registers while
 * enabling the MMU.
 */
#define DISABLE_DCACHE			(1 << 0)

/*
 * This flag marks the translation tables are Non-cacheable for MMU accesses.
 * If the flag is not specified, by default the tables are cacheable.
 */
#define XLAT_TABLE_NC			(1 << 1)

#endif /* __XLAT_TABLES_DEFS_H__ */
