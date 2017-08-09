/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cactus.h>
#include <platform_def.h>

OUTPUT_FORMAT(PLATFORM_LINKER_FORMAT)
OUTPUT_ARCH(PLATFORM_LINKER_ARCH)
ENTRY(cactus_entrypoint)


MEMORY {
    CODE_REGION (rx):
              ORIGIN = CACTUS_CODE_BASE,
              LENGTH = CACTUS_CODE_MAX_SIZE
    RODATA_REGION (r):
              ORIGIN = CACTUS_RODATA_BASE,
              LENGTH = CACTUS_RODATA_MAX_SIZE
    RWDATA_REGION (rw):
              ORIGIN = CACTUS_RWDATA_BASE,
              LENGTH = CACTUS_RWDATA_MAX_SIZE

    /* Memory pool to play with */
    TESTS_REGION (rwx):
              ORIGIN = CACTUS_TESTS_BASE,
              LENGTH = CACTUS_TESTS_SIZE
}


SECTIONS
{
    . = CACTUS_BASE;
    ASSERT(. == ALIGN(PAGE_SIZE),
           "Image start address is not aligned on a page boundary.")

    .text : {
        __TEXT_START__ = .;
        *cactus_entrypoint.o(.text*)
        *(.text*)
        *(.vectors)
        . = NEXT(PAGE_SIZE);
        __TEXT_END__ = .;
    } > CODE_REGION

    .rodata : {
        __RODATA_START__ = .;
        *(.rodata*)
        . = NEXT(PAGE_SIZE);
        __RODATA_END__ = .;
    } > RODATA_REGION

    .data : {
        __RWDATA_START__ = .;
        *(.data*)
    } > RWDATA_REGION

    .bss : {
        *(SORT_BY_ALIGNMENT(.bss*))
        *(COMMON)
        __RWDATA_END__ = .;
    } > RWDATA_REGION

    stacks (NOLOAD) : {
        __STACKS_START__ = .;
        *(tzfw_normal_stacks)
        __STACKS_END__ = .;
    } > RWDATA_REGION


    tests_memory (NOLOAD) : {
	. = . + CACTUS_TESTS_SIZE;
    } > TESTS_REGION
}
