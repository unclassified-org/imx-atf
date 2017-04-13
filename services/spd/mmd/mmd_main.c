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

#include <arch_helpers.h>
#include <assert.h>
#include <bl31.h>
#include <context_mgmt.h>
#include <debug.h>
#include <platform.h>
#include <runtime_svc.h>
#include <smcc.h>
#include <smcc_helpers.h>
#include <mm_svc.h>
#include <xlat_tables_v2.h>
#include "mmd_private.h"

/*******************************************************************************
 * MM Payload state
 ******************************************************************************/
mm_context_t mm_ctx;


/*******************************************************************************
 * This function takes an SP context pointer and:
 * 1. Applies the S-EL1 system register context from mm_ctx->cpu_ctx.
 * 2. Saves the current C runtime state (callee-saved registers) on the stack
 *    frame and saves a reference to this state.
 * 3. Calls el3_exit() so that the EL3 system and general purpose registers
 *    from the mm_ctx->cpu_ctx are used to enter the secure payload image.
 ******************************************************************************/
static uint64_t mmd_synchronous_sp_entry(mm_context_t *mm_ctx)
{
	uint64_t rc;

	assert(mm_ctx != NULL);
	assert(mm_ctx->c_rt_ctx == 0);
	assert(cm_get_context(SECURE) == &mm_ctx->cpu_ctx);

	/* Apply the Secure EL1 system register context and switch to it */
	cm_el1_sysregs_context_restore(SECURE);
	cm_set_next_eret_context(SECURE);

	VERBOSE("%s: We're about to enter the MM payload...\n", __func__);

	rc = mmd_enter_sp(&mm_ctx->c_rt_ctx);
#if DEBUG
	mm_ctx->c_rt_ctx = 0;
#endif

	return rc;
}


/*******************************************************************************
 * This function takes an MM context pointer and:
 * 1. Saves the S-EL1 system register context tp mm_ctx->cpu_ctx.
 * 2. Restores the current C runtime state (callee saved registers) from the
 *    stack frame using the reference to this state saved in mmd_enter_sp().
 * 3. It does not need to save any general purpose or EL3 system register state
 *    as the generic smc entry routine should have saved those.
 ******************************************************************************/
static void mmd_synchronous_sp_exit(mm_context_t *mm_ctx, uint64_t ret)
{
	assert(mm_ctx != NULL);
	/* Save the Secure EL1 system register context */
	assert(cm_get_context(SECURE) == &mm_ctx->cpu_ctx);
	cm_el1_sysregs_context_save(SECURE);

	assert(mm_ctx->c_rt_ctx != 0);
	mmd_exit_sp(mm_ctx->c_rt_ctx, ret);

	/* Should never reach here */
	assert(0);
}

/*******************************************************************************
 * This function passes control to the Secure Payload image (BL32) for the first
 * time on the primary cpu after a cold boot. It assumes that a valid secure
 * context has already been created by mmd_setup() which can be directly
 * used. This function performs a synchronous entry into the Secure payload.
 * The SP passes control back to this routine through a SMC.
 ******************************************************************************/
int32_t mmd_init(void)
{
	entry_point_info_t *mm_entry_point;
	uint64_t rc;

	VERBOSE("%s entry\n", __func__);

	/*
	 * Get information about the Secure Payload (BL32) image. Its
	 * absence is a critical failure.
	 */
	mm_entry_point = bl31_plat_get_next_image_ep_info(SECURE);
	assert(mm_entry_point);

	cm_init_my_context(mm_entry_point);

	/*
	 * Arrange for an entry into the secure payload.
	 */
	rc = mmd_synchronous_sp_entry(&mm_ctx);
	assert(rc != 0);

	return rc;
}

/*******************************************************************************
 * Given a secure payload entrypoint info pointer, entry point PC & pointer to
 * a context data structure, this function will initialize the MM context and
 * entry point info for the secure payload
 ******************************************************************************/
static void mmd_init_mm_ep_state(struct entry_point_info *mm_entry_point,
				uint64_t pc,
				mm_context_t *mm_ctx)
{
	uint32_t ep_attr;

	assert(mm_entry_point);
	assert(pc);
	assert(mm_ctx);

	cm_set_context(&mm_ctx->cpu_ctx, SECURE);

	/* initialise an entrypoint to set up the CPU context */
	ep_attr = SECURE | EP_ST_ENABLE;
	if (read_sctlr_el3() & SCTLR_EE_BIT)
		ep_attr |= EP_EE_BIG;
	SET_PARAM_HEAD(mm_entry_point, PARAM_EP, VERSION_1, ep_attr);

	mm_entry_point->pc = pc;
	/* The MM payload runs in S-EL0 */
	mm_entry_point->spsr = SPSR_64(MODE_EL0,
					MODE_SP_EL0,
					DISABLE_ALL_EXCEPTIONS);

	zeromem(&mm_entry_point->args, sizeof(mm_entry_point->args));
}

/*******************************************************************************
 * Secure Payload Dispatcher setup. The SPD finds out the SP entrypoint if not
 * already known and initialises the context for entry into the SP for its
 * initialisation.
 ******************************************************************************/
int32_t mmd_setup(void)
{
	entry_point_info_t *mm_ep_info;

	VERBOSE("%s entry\n", __func__);

	/*
	 * Get information about the Secure Payload (BL32) image. Its
	 * absence is a critical failure.
	 */
	mm_ep_info = bl31_plat_get_next_image_ep_info(SECURE);
	if (!mm_ep_info) {
		WARN("No MM provided by BL2 boot loader, Booting device"
			" without MM initialization. SMCs destined for MM"
			" will return SMC_UNK\n");
		return 1;
	}

	/*
	 * If there's no valid entry point for SP, we return a non-zero value
	 * signalling failure initializing the service. We bail out without
	 * registering any handlers
	 */
	if (!mm_ep_info->pc)
		return 1;

	mmd_init_mm_ep_state(mm_ep_info, mm_ep_info->pc, &mm_ctx);

	/*
	 * All MMD initialization done. Now register our init function with
	 * BL31 for deferred invocation
	 */
	bl31_register_bl32_init(&mmd_init);
	VERBOSE("%s exit\n", __func__);
	return 0;
}

/*
 * Attributes are encoded using in a different format in the
 * MM_MEMORY_ATTRIBUTES_SET SMC than in TF, where we use the mmap_attr_t
 * enum type.
 * This function converts an attributes value from the SMC format to the
 * mmap_attr_t format.
 */
static int smc_attr_to_mmap_attr(int attributes)
{
	/* Base attributes. Can't change these through the SMC */
	int tf_attr = MT_MEMORY | MT_SECURE;

	/*
	 * TODO: Properly define bit shifts and masks instead of using magic
	 * values
	 */
	if ((attributes & 3) == 1)
		tf_attr |= MT_RW;
	if (((attributes >> 2) & 1) == 1)
		tf_attr |= MT_EXECUTE_NEVER;

	return tf_attr;
}

static int mm_memory_attributes_smc_handler(u_register_t page_address,
					u_register_t pages_count,
					u_register_t smc_attributes)
{
	VERBOSE("Received MM_MEMORY_ATTRIBUTES_SET SMC\n");

	uintptr_t base_va = (uintptr_t) page_address;
	size_t size = (size_t) (pages_count * PAGE_SIZE);
	int attributes = (int) smc_attributes;
	VERBOSE("  Start address  : 0x%lx\n", base_va);
	VERBOSE("  Number of pages: %i (%zi bytes)\n",
		(int) pages_count, size);
	VERBOSE("  Attributes     : 0x%x\n", attributes);
	attributes = smc_attr_to_mmap_attr(attributes);
	VERBOSE("  (Equivalent TF attributes: 0x%x)\n", attributes);

	return change_mem_attributes(mm_shim_xlat_ctx_handle,
				base_va, size, attributes);
}


uint64_t mmd_smc_handler(uint32_t smc_fid,
			 uint64_t x1,
			 uint64_t x2,
			 uint64_t x3,
			 uint64_t x4,
			 void *cookie,
			 void *handle,
			 uint64_t flags)
{
	assert(handle == cm_get_context(SECURE));

	switch (smc_fid) {
	case MM_INIT_COMPLETE_AARCH64:
		/*
		 * MM reports completion. The MMD must have initiated the
		 * original request through a synchronous entry into the MM
		 * payload. Jump back to the original C runtime context.
		 */
		mmd_synchronous_sp_exit(&mm_ctx, x1);

	case MM_MEMORY_ATTRIBUTES_SET:
		SMC_RET1(handle,
			mm_memory_attributes_smc_handler(x1, x2, x3));

	default:
		/* TODO: Implement me */
		SMC_RET1(handle, SMC_UNK);
	}
}
