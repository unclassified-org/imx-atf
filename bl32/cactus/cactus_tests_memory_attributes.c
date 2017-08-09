/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <bl_common.h>
#include <cactus.h>
#include <debug.h>
#include <errno.h>
#include <spm_svc.h>
#include <types.h>

/* Data access permissions */
#define DATA_AP_NO_ACCESS	0
#define DATA_AP_RW		1
/* Value 2 is reserved */
#define DATA_AP_RO		3

/* Instruction access permissions */
#define INSTR_AP_EXEC		0
#define INSTR_AP_NON_EXEC	1

/*
 * Given the required instruction and data access permissions,
 * create a memory access controls value that is formatted as expected
 * by the MM_MEMORY_ATTRIBUTES_SET SMC.
 */
static inline uint32_t mem_access_perm(int instr_access_perm,
				int data_access_perm)
{
	return ((instr_access_perm & 1) << 2) |
	       ((data_access_perm & 3) << 0);
}

/*
 * Error codes of the SP_MEM_ATTRIBUTES_SET_AARCH64 SVC
 * XXX: I made up these values. Where are the official values specified?
 */
#define SUCCESS			0
#define INVALID_PARAMETERS	(-EINVAL)
#define NOT_SUPPORTED		(-2)
#define DENIED			(-EPERM)
#define NO_MEMORY		(-ENOMEM)

/* Compute the size in bytes equivalent to the given number of pages */
#define PAGES_TO_BYTES(pages)	((pages) * PAGE_SIZE)

/*
 * XXX: This definition is coming from bl_common.h but it's not exported
 * when IMAGE_LOAD_V2 is enabled.
 */
uintptr_t page_align(uintptr_t value, unsigned dir);

/*
 * Send an SP_MEM_ATTRIBUTES_SET_AARCH64 SVC with the given arguments.
 * Return the return value of the SVC.
 */
static int32_t request_mem_attr_changes(uintptr_t base_address,
					int pages_count,
					uint32_t memory_access_controls)
{
	INFO("Requesting memory attributes change\n");
	INFO("  Start address  : %p\n", (void *) base_address);
	INFO("  Number of pages: %i\n", pages_count);
	INFO("  Attributes     : 0x%x\n", memory_access_controls);

	uint64_t ret = cactus_svc(SP_MEM_ATTRIBUTES_SET_AARCH64,
		base_address,
		pages_count,
		memory_access_controls,
		0, 0, 0, 0);

	return (int32_t) ret;
}

/*
 * Description string for the current test.
 * This variable is meant to be used through announce_test_start/end().
 */
static const char *cur_test_desc = NULL;

static void announce_test_start(const char *test_desc)
{
	cur_test_desc = test_desc;
	tf_printf("\n\n[+] %s\n", test_desc);
}

static void announce_test_end(void)
{
	tf_printf("\nTest \"%s\" passed.\n", cur_test_desc);
	cur_test_desc = NULL;
}

/*
 * This function expects a base address and number of pages identifying the
 * extents of some memory region mapped as non-executable, read-only.
 *
 * 1) It changes its data access permissions to read-write.
 * 2) It checks this memory can now be written to.
 * 3) It restores the original data access permissions.
 *
 * If any check fails, it loops forever. It could also trigger a permission
 * fault while trying to write to the memory.
 */
static void mem_attr_changes_unittest(uintptr_t addr, int pages_count)
{
	int32_t ret;
	uintptr_t end_addr = addr + pages_count * PAGE_SIZE;

	char test_desc[50];
	snprintf(test_desc, sizeof(test_desc),
		"RO -> RW (%i page(s) from address 0x%lx)", pages_count, addr);
	announce_test_start(test_desc);

	/*
	 * Ensure we don't change the attributes of some random memory
	 * location
	 */
	assert(addr >= CACTUS_TESTS_BASE);
	assert(end_addr < (CACTUS_TESTS_BASE + CACTUS_TESTS_SIZE));

	/* See mm_stub_setup.c for the original attributes */
	uint32_t old_attr = mem_access_perm(INSTR_AP_NON_EXEC, DATA_AP_RO);
	/* Memory was read-only, let's try changing that to RW */
	uint32_t new_attr = mem_access_perm(INSTR_AP_NON_EXEC, DATA_AP_RW);

	ret = request_mem_attr_changes(addr, pages_count, new_attr);
	expect(ret, SUCCESS);
	tf_printf("Successfully changed memory attributes\n");

	/* If it worked, we should be able to write to this memory now! */
	for (unsigned char *data = (unsigned char *) addr;
	     (uintptr_t) data != end_addr;
	     ++data) {
		*data = 42;
	}
	tf_printf("Successfully wrote to the memory\n");

	/* Let's revert back to the original attributes for the next test */
	ret = request_mem_attr_changes(addr, pages_count, old_attr);
	expect(ret, SUCCESS);
	tf_printf("Successfully restored the old attributes\n");

	announce_test_end();
}

/*
 * Exercise the ability of the Trusted Firmware to change the data access
 * permissions and instruction execution permissions of some memory region.
 */
void mem_attr_changes_tests(void)
{
	uint32_t attributes;
	int32_t ret;
	uintptr_t addr;

	tf_printf("\n\n");
	tf_printf("========================================\n");
	tf_printf("Starting memory attributes changes tests\n");
	tf_printf("========================================\n");

	/*
	 * Start with error cases, i.e. requests that are expected to be denied
	 */
	announce_test_start("Read-write, executable");
	attributes = mem_access_perm(INSTR_AP_EXEC, DATA_AP_RW);
	ret = request_mem_attr_changes(RWDATA_SECTION_START, 1, attributes);
	expect(ret, INVALID_PARAMETERS);
	announce_test_end();

	announce_test_start("Size == 0");
	attributes = mem_access_perm(INSTR_AP_NON_EXEC, DATA_AP_RW);
	ret = request_mem_attr_changes(RWDATA_SECTION_START, 0, attributes);
	expect(ret, INVALID_PARAMETERS);
	announce_test_end();

	announce_test_start("Unaligned address");
	attributes = mem_access_perm(INSTR_AP_NON_EXEC, DATA_AP_RW);
	/*
	 * Choose some random address and make sure it is not aligned on a
	 * page boundary.
	 */
	addr = bound_rand(CACTUS_TESTS_BASE, CACTUS_TESTS_END);
	if (IS_PAGE_ALIGNED(addr))
		++addr;
	ret = request_mem_attr_changes(addr, 1, attributes);
	expect(ret, INVALID_PARAMETERS);
	announce_test_end();

	announce_test_start("Unmapped memory region");
	addr = CACTUS_TESTS_END + 2 * PAGE_SIZE;
	attributes = mem_access_perm(INSTR_AP_NON_EXEC, DATA_AP_RW);
	ret = request_mem_attr_changes(addr, 3, attributes);
	expect(ret, INVALID_PARAMETERS);
	announce_test_end();

	announce_test_start("Partially unmapped memory region");
	addr = CACTUS_TESTS_END - 2 * PAGE_SIZE;
	attributes = mem_access_perm(INSTR_AP_NON_EXEC, DATA_AP_RW);
	ret = request_mem_attr_changes(addr, 6, attributes);
	expect(ret, INVALID_PARAMETERS);
	announce_test_end();

	/*
	 * Now try some requests that are supposed to be allowed.
	 */
	for (unsigned int i = 0; i < 20; ++i) {
		/*
		 * Choose some random address in the pool of memory reserved
		 * for these tests.
		 */
		const int pages_max = CACTUS_TESTS_SIZE / PAGE_SIZE;
		int pages_count = bound_rand(1, pages_max);

		addr = bound_rand(
			CACTUS_TESTS_BASE,
			CACTUS_TESTS_END - PAGES_TO_BYTES(pages_count));
		addr = page_align(addr, DOWN);

		mem_attr_changes_unittest(addr, pages_count);
	}

	tf_printf("\n\n");
	tf_printf("========================================\n");
	tf_printf("End of memory attributes changes tests\n");
	tf_printf("========================================\n\n");
}
