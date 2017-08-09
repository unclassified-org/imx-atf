/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __CACTUS_H__
#define __CACTUS_H__

#include <platform_def.h>

/*
 * These values are used to drive the linker script.
 */
#define CACTUS_BASE		BL32_BASE

#define CACTUS_CODE_BASE	CACTUS_BASE
#define CACTUS_CODE_MAX_SIZE	(2 * PAGE_SIZE)

#define CACTUS_RODATA_BASE	(CACTUS_CODE_BASE + CACTUS_CODE_MAX_SIZE)
#define CACTUS_RODATA_MAX_SIZE	(1 * PAGE_SIZE)

/* Read-write data and stack */
#define CACTUS_RWDATA_BASE					\
	(CACTUS_RODATA_BASE + CACTUS_RODATA_MAX_SIZE)
#define CACTUS_RWDATA_MAX_SIZE	(2 * PAGE_SIZE)

/*
 * Reserve a memory pool at the end of the image to experiment with memory
 * attributes changes.
 */
#define CACTUS_TESTS_BASE	(CACTUS_RWDATA_BASE + CACTUS_RWDATA_MAX_SIZE)
#define CACTUS_TESTS_SIZE	(15 * PAGE_SIZE)
#define CACTUS_TESTS_END	(CACTUS_TESTS_BASE + CACTUS_TESTS_SIZE)

#ifndef __LINKER__
#include <types.h>

/*
 * Linker symbols to figure out the actual layout of the image.
 */
extern uintptr_t __TEXT_START__;
extern uintptr_t __TEXT_END__;
#define CODE_SECTION_START	((uint64_t) &__TEXT_START__)
#define CODE_SECTION_END	((uint64_t) &__TEXT_END__)
#define CODE_SECTION_SIZE	(CODE_SECTION_END - CODE_SECTION_START)

extern uintptr_t __RODATA_START__;
extern uintptr_t __RODATA_END__;
#define RODATA_SECTION_START	((uint64_t) &__RODATA_START__)
#define RODATA_SECTION_END	((uint64_t) &__RODATA_END__)
#define RODATA_SECTION_SIZE	(RODATA_SECTION_END - RODATA_SECTION_START)

extern uintptr_t __RWDATA_START__;
extern uintptr_t __RWDATA_END__;
#define RWDATA_SECTION_START	((uint64_t) &__RWDATA_START__)
#define RWDATA_SECTION_END	((uint64_t) &__RWDATA_END__)
#define RWDATA_SECTION_SIZE	(RWDATA_SECTION_END - RWDATA_SECTION_START)

/*
 * Helper functions
 */
uint64_t cactus_svc(uint64_t x0, uint64_t x1, uint64_t x2, uint64_t x3,
		uint64_t x4, uint64_t x5, uint64_t x6, uint64_t x7);

/*
 * Choose a pseudo-random number within the [min,max] range (both limits are
 * inclusive).
 */
uintptr_t bound_rand(uintptr_t min, uintptr_t max);

/*
 * Check that expr == expected.
 * If not, loop forever.
 */
void expect(int expr, int expected);

void mem_attr_changes_tests(void);

#endif /* __LINKER__ */

#endif /* __CACTUS_H__ */
