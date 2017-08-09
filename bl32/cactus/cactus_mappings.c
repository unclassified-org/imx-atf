/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file provides the memory regions that the MM dispatcher must map in the
 * S-EL1&0 translation regime for our test S-EL0 Test Payload to work properly.
 */

#include <arm_xlat_tables.h>
#include "cactus.h"

/*
 * Memory mappings for our test image
 */
#define SEL0_IMAGE_CODE							\
	MAP_REGION_FLAT(						\
		CACTUS_CODE_BASE,					\
		CACTUS_CODE_MAX_SIZE,					\
		MT_CODE | MT_SECURE)

#define SEL0_IMAGE_RODATA						\
	MAP_REGION_FLAT(						\
		CACTUS_RODATA_BASE,					\
		CACTUS_RODATA_MAX_SIZE,					\
		MT_RO_DATA | MT_SECURE)

#define SEL0_IMAGE_RWDATA						\
	MAP_REGION_FLAT(						\
		CACTUS_RWDATA_BASE,					\
		CACTUS_RWDATA_MAX_SIZE,					\
		MT_MEMORY | MT_SECURE | MT_RW | MT_EXECUTE_NEVER)

/*
 * This last region must be mapped at a page granularity because we'll attempt
 * to change its memory attributes later. Initially map it as read-only data.
 */
#define SEL0_IMAGE_TESTS_MEMORY						\
	MAP_REGION_GRANULARITY(						\
		CACTUS_TESTS_BASE, CACTUS_TESTS_BASE,			\
		CACTUS_TESTS_SIZE,					\
		MT_RO_DATA | MT_SECURE,					\
		PAGE_SIZE)

const mmap_region_t plat_arm_secure_partition_mmap[] = {
	V2M_MAP_IOFPGA, /* for the UART */
	SEL0_IMAGE_CODE,
	SEL0_IMAGE_RODATA,
	SEL0_IMAGE_RWDATA,
	SEL0_IMAGE_TESTS_MEMORY,
	{0}
};
