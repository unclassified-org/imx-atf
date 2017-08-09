/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <console.h>
#include <debug.h>
#include <cactus.h>
#include <platform_def.h>
#include <plat_arm.h>
#include <std_svc.h>


static void cactus_print_memory_layout(void)
{
	NOTICE("Cactus memory layout:\n");
	NOTICE("  Overall image         : 0x%llx - 0x%llx  (%llu bytes)\n",
		CACTUS_BASE, CACTUS_TESTS_END, CACTUS_TESTS_END - CACTUS_BASE);
	NOTICE("  Code region           : 0x%llx - 0x%llx  (%u bytes)\n",
		CACTUS_CODE_BASE, CACTUS_CODE_BASE + CACTUS_CODE_MAX_SIZE,
		CACTUS_CODE_MAX_SIZE);
	NOTICE("  Read-only data region : 0x%llx - 0x%llx  (%u bytes)\n",
		CACTUS_RODATA_BASE, CACTUS_RODATA_BASE + CACTUS_RODATA_MAX_SIZE,
		CACTUS_RODATA_MAX_SIZE);
	NOTICE("  Read-write data region: 0x%llx - 0x%llx  (%u bytes)\n",
		CACTUS_RWDATA_BASE, CACTUS_RWDATA_BASE + CACTUS_RWDATA_MAX_SIZE,
		CACTUS_RWDATA_MAX_SIZE);
	NOTICE("  Memory pool for tests : 0x%llx - 0x%llx  (%u bytes)\n",
		CACTUS_TESTS_BASE, CACTUS_TESTS_END, CACTUS_TESTS_SIZE);
}


int cactus_main(void)
{
	console_init(PLAT_ARM_BOOT_UART_BASE,
		PLAT_ARM_BOOT_UART_CLK_IN_HZ,
		ARM_CONSOLE_BAUDRATE);

	NOTICE("%s() entry\n", __func__);

	cactus_print_memory_layout();

	mem_attr_changes_tests();

	return 0;
}
