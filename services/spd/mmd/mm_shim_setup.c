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
#include <context.h>
#include <context_mgmt.h>
#include <common_def.h>
#include <debug.h>
#include <platform_def.h>
#include <types.h>
#include <xlat_tables_v2.h>

#include "mm_shim.h"

void mm_shim_setup(void)
{
	VERBOSE("S-EL1/S-EL0 context setup start...\n");

	/* Assert we are in Secure state. */
	assert((read_scr_el3() & SCR_NS_BIT) == 0);

	/*
	 * Setup translation tables.
	 */
	mmap_region_t sel1_exception_vectors =
		MAP_REGION_GRANULARITY(MM_SHIM_EXCEPTIONS_BASE,
				MM_SHIM_EXCEPTIONS_BASE,
				MM_SHIM_EXCEPTIONS_SIZE,
				MT_CODE | MT_SECURE,
				MM_SHIM_EXCEPTIONS_SIZE);
	mmap_add_region_ctx(mm_shim_xlat_ctx_handle,
			&sel1_exception_vectors);

	init_xlat_tables_ctx(mm_shim_xlat_ctx_handle);

	VERBOSE("S-EL1/S-EL0 context setup end.\n");
}

void mm_shim_prepare_context(void)
{
	VERBOSE("Updating S-EL1/S-EL0 context registers.\n");

	cpu_context_t *ctx = cm_get_context(SECURE);

	assert(ctx);

	/* MMU-related registers */

	uint64_t mair_el1, tcr_el1, ttbr_el1, sctlr_el1;

	mm_shim_prepare_mmu_context_el1(&mair_el1, &tcr_el1, &ttbr_el1,
					&sctlr_el1);

	sctlr_el1 |= SCTLR_UCI_BIT | SCTLR_NTWE_BIT | SCTLR_NTWI_BIT |
		     SCTLR_UCT_BIT | SCTLR_DZE_BIT | SCTLR_I_BIT |
		     SCTLR_UMA_BIT | SCTLR_SA0_BIT | SCTLR_A_BIT;
	sctlr_el1 &= ~SCTLR_E0E_BIT;

	write_ctx_reg(get_sysregs_ctx(ctx), CTX_SCTLR_EL1, sctlr_el1);
	write_ctx_reg(get_sysregs_ctx(ctx), CTX_TTBR0_EL1, ttbr_el1);
	write_ctx_reg(get_sysregs_ctx(ctx), CTX_MAIR_EL1, mair_el1);
	write_ctx_reg(get_sysregs_ctx(ctx), CTX_TCR_EL1, tcr_el1);

	/* Other system registers */

	write_ctx_reg(get_sysregs_ctx(ctx), CTX_VBAR_EL1, MM_SHIM_EXCEPTIONS_BASE_PTR);

	uint64_t cpacr_el1 = read_ctx_reg(get_sysregs_ctx(ctx), CTX_CPACR_EL1);
	cpacr_el1 |= CPACR_EL1_FPEN(CPACR_EL1_FP_TRAP_NONE);
	write_ctx_reg(get_sysregs_ctx(ctx), CTX_CPACR_EL1, cpacr_el1);

	/* General-Purpose registers */

	/*
	 * X0: Virtual address of a buffer shared between EL3 and Secure EL0.
	 *     The buffer will be mapped in the Secure EL1 translation regime
	 *     with Normal IS WBWA attributes and RO data and Execute Never
	 *     instruction access permissions.
	 *
	 * X1: Size of the buffer in bytes
	 *
	 * X2: cookie value (Implementation Defined)
	 *
	 * X3: cookie value (Implementation Defined)
	 */

	/*
	write_ctx_reg(get_gpregs_ctx(ctx), CTX_GPREG_X0, buffer_base); ### XXX : TODO
	write_ctx_reg(get_gpregs_ctx(ctx), CTX_GPREG_X1, buffer_size); ### XXX : TODO
	write_ctx_reg(get_gpregs_ctx(ctx), CTX_GPREG_X2, cookie_1);
	write_ctx_reg(get_gpregs_ctx(ctx), CTX_GPREG_X3, cookie_2);
	*/

	/* X4 to X30 = 0, done by cm_init_my_context() */

	/*
	 * SP_EL0: A non-zero value will indicate that the Dispatcher has
	 * initialized the stack pointer for the current CPU through
	 * implementation defined means. The value will be 0 otherwise.
	 */
	write_ctx_reg(get_gpregs_ctx(ctx), CTX_GPREG_SP_EL0, 0);

	/*
	 * PSTATE
	 * D,A,I,F=1
	 * SpSel = 0 ### XXX : TODO
	 * NRW = 0 ### XXX : TODO
	 */

	/* XXX : TODO : Does this go here or in mm_shim_exceptions.S ? I'd say
	 * this goes here in case the payload wants to change it, but I'm not
	 * sure.
	 */
	write_daifset(DAIF_FIQ_BIT | DAIF_IRQ_BIT | DAIF_ABT_BIT | DAIF_DBG_BIT);

}
