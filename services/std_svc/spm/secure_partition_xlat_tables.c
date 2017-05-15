/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <arch.h>
#include <arch_helpers.h>
#include <assert.h>
#include <common_def.h>
#include <platform_def.h>
#include <secure_partition.h>
#include <string.h>
#include <types.h>
#include <utils.h>
#include <xlat_tables_v2.h>
#include "../../../lib/xlat_tables_v2/aarch64/xlat_tables_arch.h"
#include "../../../lib/xlat_tables_v2/xlat_tables_private.h"
#include "secure_partition_private.h"

static mmap_region_t secure_partition_mmap[SECURE_PARTITION_MMAP_REGIONS + 1];

static uint64_t
secure_partition_xlat_tables[SECURE_PARTITION_MAX_XLAT_TABLES][XLAT_TABLE_ENTRIES]
__aligned(XLAT_TABLE_SIZE)
__section("secure_partition_xlat_table");

static uint64_t secure_partition_base_xlat_table[NUM_BASE_LEVEL_ENTRIES]
__aligned(NUM_BASE_LEVEL_ENTRIES * sizeof(uint64_t))
__section("secure_partition_xlat_table_base");

#if PLAT_XLAT_TABLES_DYNAMIC
static int
secure_partition_xlat_tables_mapped_regions[SECURE_PARTITION_XLAT_TABLES];
#endif /* PLAT_XLAT_TABLES_DYNAMIC */

static xlat_ctx_t secure_partition_xlat_ctx = {

	.pa_max_address = PLAT_PHY_ADDR_SPACE_SIZE - 1,
	.va_max_address = PLAT_VIRT_ADDR_SPACE_SIZE - 1,

	.mmap = secure_partition_mmap,
	.mmap_num = SECURE_PARTITION_MMAP_REGIONS,

	.tables = secure_partition_xlat_tables,
	.tables_num = SECURE_PARTITION_MAX_XLAT_TABLES,
#if PLAT_XLAT_TABLES_DYNAMIC
	.tables_mapped_regions = secure_partition_xlat_tables_mapped_regions,
#endif /* PLAT_XLAT_TABLES_DYNAMIC */

	.base_table = secure_partition_base_xlat_table,
	.base_table_entries = NUM_BASE_LEVEL_ENTRIES,

	.max_pa = 0,
	.max_va = 0,

	.next_table = 0,

	.base_level = XLAT_TABLE_LEVEL_BASE,

	.initialized = 0
};

/* Export a handle on the secure partition translation context */
xlat_ctx_handle_t secure_partition_xlat_ctx_handle = &secure_partition_xlat_ctx;

static unsigned long long calc_physical_addr_size_bits(
					unsigned long long max_addr)
{
	/* Physical address can't exceed 48 bits */
	assert((max_addr & ADDR_MASK_48_TO_63) == 0);

	/* 48 bits address */
	if (max_addr & ADDR_MASK_44_TO_47)
		return TCR_PS_BITS_256TB;

	/* 44 bits address */
	if (max_addr & ADDR_MASK_42_TO_43)
		return TCR_PS_BITS_16TB;

	/* 42 bits address */
	if (max_addr & ADDR_MASK_40_TO_41)
		return TCR_PS_BITS_4TB;

	/* 40 bits address */
	if (max_addr & ADDR_MASK_36_TO_39)
		return TCR_PS_BITS_1TB;

	/* 36 bits address */
	if (max_addr & ADDR_MASK_32_TO_35)
		return TCR_PS_BITS_64GB;

	return TCR_PS_BITS_4GB;
}

void secure_partition_prepare_xlat_context(uint64_t *mair, uint64_t *tcr,
					   uint64_t *ttbr, uint64_t *sctlr)
{
	assert(mair && tcr && ttbr && sctlr);

	/* Set attributes in the right indices of the MAIR */
	*mair = MAIR_ATTR_SET(ATTR_DEVICE, ATTR_DEVICE_INDEX);
	*mair |= MAIR_ATTR_SET(ATTR_IWBWA_OWBWA_NTR,
			       ATTR_IWBWA_OWBWA_NTR_INDEX);
	*mair |= MAIR_ATTR_SET(ATTR_NON_CACHEABLE,
			       ATTR_NON_CACHEABLE_INDEX);

	/* Invalidate TLBs at the target exception level */
	tlbivmalle1();

	/* Set TCR bits as well. */
	/* Inner & outer WBWA & shareable + T0SZ = 32 */
	unsigned long long tcr_ps_bits;
	tcr_ps_bits = calc_physical_addr_size_bits(PLAT_PHY_ADDR_SPACE_SIZE);
	*tcr = TCR_SH_INNER_SHAREABLE | TCR_RGN_OUTER_WBA |
		TCR_RGN_INNER_WBA |
		(64 - __builtin_ctzl(PLAT_VIRT_ADDR_SPACE_SIZE));
	*tcr |= tcr_ps_bits << TCR_EL1_IPS_SHIFT;

	/* Set TTBR bits as well */
	*ttbr = (uint64_t) secure_partition_base_xlat_table;

	*sctlr = SCTLR_EL1_RES1;
	*sctlr |= SCTLR_WXN_BIT | SCTLR_C_BIT | SCTLR_M_BIT;
}
