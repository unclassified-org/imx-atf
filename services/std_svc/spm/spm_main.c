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
#include <secure_partition.h>
#include <smcc.h>
#include <smcc_helpers.h>
#include <spm_svc.h>
#include <xlat_tables_v2.h>
#include "spm_private.h"

/*******************************************************************************
 * Secure Partition context information.
 ******************************************************************************/
static secure_partition_context_t sp_ctx[PLATFORM_CORE_COUNT];

/*******************************************************************************
 * Replace the S-EL1 re-entry information with S-EL0 re-entry
 * information
 ******************************************************************************/
void spm_setup_next_eret_into_sel0(cpu_context_t *secure_context)
{
	unsigned long elr_el1;
	unsigned int spsr_el1;

	assert(secure_context == cm_get_context(SECURE));
	elr_el1 = read_elr_el1();
	spsr_el1 = read_spsr_el1();

	cm_set_elr_spsr_el3(SECURE, elr_el1, spsr_el1);
	return;
}

/*******************************************************************************
 * This function takes an SP context pointer and:
 * 1. Applies the S-EL1 system register context from sp_ctx->cpu_ctx.
 * 2. Saves the current C runtime state (callee-saved registers) on the stack
 *    frame and saves a reference to this state.
 * 3. Calls el3_exit() so that the EL3 system and general purpose registers
 *    from the sp_ctx->cpu_ctx are used to enter the secure payload image.
 ******************************************************************************/
static uint64_t spm_synchronous_sp_entry(secure_partition_context_t *sp_ctx_ptr)
{
	uint64_t rc;

	assert(sp_ctx_ptr != NULL);
	assert(sp_ctx_ptr->c_rt_ctx == 0);
	assert(cm_get_context(SECURE) == &sp_ctx_ptr->cpu_ctx);

	/* Apply the Secure EL1 system register context and switch to it */
	cm_el1_sysregs_context_restore(SECURE);
	cm_set_next_eret_context(SECURE);

	VERBOSE("%s: We're about to enter the Secure partition...\n", __func__);

	rc = spm_secure_partition_enter(&sp_ctx_ptr->c_rt_ctx);
#if DEBUG
	sp_ctx_ptr->c_rt_ctx = 0;
#endif

	return rc;
}


/*******************************************************************************
 * This function takes a Secure partition context pointer and:
 * 1. Saves the S-EL1 system register context tp sp_ctx->cpu_ctx.
 * 2. Restores the current C runtime state (callee saved registers) from the
 *    stack frame using the reference to this state saved in
 *    spm_secure_partition_enter().
 * 3. It does not need to save any general purpose or EL3 system register state
 *    as the generic smc entry routine should have saved those.
 ******************************************************************************/
static void spm_synchronous_sp_exit(secure_partition_context_t *sp_ctx_ptr,
				    uint64_t ret)
{
	assert(sp_ctx_ptr != NULL);
	/* Save the Secure EL1 system register context */
	assert(cm_get_context(SECURE) == &sp_ctx_ptr->cpu_ctx);
	cm_el1_sysregs_context_save(SECURE);

	assert(sp_ctx_ptr->c_rt_ctx != 0);
	spm_secure_partition_exit(sp_ctx_ptr->c_rt_ctx, ret);

	/* Should never reach here */
	assert(0);
}

/*******************************************************************************
 * This function passes control to the Secure Payload image (BL32) for the first
 * time on the primary cpu after a cold boot. It assumes that a valid secure
 * context has already been created by spm_setup() which can be directly
 * used. This function performs a synchronous entry into the Secure payload.
 * The SP passes control back to this routine through a SMC.
 ******************************************************************************/
int32_t spm_init(void)
{
	entry_point_info_t *secure_partition_ep_info;
	uint64_t rc;
	unsigned int linear_id;

	VERBOSE("%s entry\n", __func__);
	linear_id = plat_my_core_pos();


	/*
	 * Get information about the Secure Payload (BL32) image. Its
	 * absence is a critical failure.
	 */
	secure_partition_ep_info = bl31_plat_get_next_image_ep_info(SECURE);
	assert(secure_partition_ep_info);

	/*
	 * Initialise the common context and then overlay the S-EL0 specific
	 * context on top of it.
	 */
	cm_init_my_context(secure_partition_ep_info);
	secure_partition_prepare_context();

	/*
	 * Set the state of the Secure partition and arrange for an entry into
	 * the secure payload.
	 */
	set_sp_pstate(sp_ctx[linear_id].flags, SP_PSTATE_OFF);
	rc = spm_synchronous_sp_entry(&sp_ctx[linear_id]);
	assert(rc == 0);

	/* Mark the partition as being ON on this CPU */
	set_sp_pstate(sp_ctx[linear_id].flags, SP_PSTATE_ON);
	return rc;
}

/*******************************************************************************
 * Given a secure payload entrypoint info pointer, entry point PC & pointer to
 * a context data structure, this function will initialize the SPM context and
 * entry point info for the secure payload
 ******************************************************************************/
void spm_init_sp_ep_state(struct entry_point_info *sp_ep_info,
			  uint64_t pc,
			  secure_partition_context_t *sp_ctx_ptr)
{
	uint32_t ep_attr;

	assert(sp_ep_info);
	assert(pc);
	assert(sp_ctx_ptr);

	cm_set_context(&sp_ctx_ptr->cpu_ctx, SECURE);

	/* initialise an entrypoint to set up the CPU context */
	ep_attr = SECURE | EP_ST_ENABLE;
	if (read_sctlr_el3() & SCTLR_EE_BIT)
		ep_attr |= EP_EE_BIG;
	SET_PARAM_HEAD(sp_ep_info, PARAM_EP, VERSION_1, ep_attr);

	sp_ep_info->pc = pc;
	/* The SPM payload runs in S-EL0 */
	sp_ep_info->spsr = SPSR_64(MODE_EL0,
				   MODE_SP_EL0,
				   DISABLE_ALL_EXCEPTIONS);

	zeromem(&sp_ep_info->args, sizeof(sp_ep_info->args));
}

/*******************************************************************************
 * Secure Payload Dispatcher setup. The SPD finds out the SP entrypoint if not
 * already known and initialises the context for entry into the SP for its
 * initialisation.
 ******************************************************************************/
int32_t spm_setup(void)
{
	entry_point_info_t *secure_partition_ep_info;
	unsigned int linear_id;

	VERBOSE("%s entry\n", __func__);
	linear_id = plat_my_core_pos();

	/*
	 * Get information about the Secure Payload (BL32) image. Its
	 * absence is a critical failure.
	 */
	secure_partition_ep_info = bl31_plat_get_next_image_ep_info(SECURE);
	if (!secure_partition_ep_info) {
		WARN("No SPM provided by BL2 boot loader, Booting device"
			" without SPM initialization. SMCs destined for SPM"
			" will return SMC_UNK\n");
		return 1;
	}

	/*
	 * If there's no valid entry point for SP, we return a non-zero value
	 * signalling failure initializing the service. We bail out without
	 * registering any handlers
	 */
	if (!secure_partition_ep_info->pc)
		return 1;

	spm_init_sp_ep_state(secure_partition_ep_info,
			      secure_partition_ep_info->pc,
			      &sp_ctx[linear_id]);

	/*
	 * Setup translation tables and calculate values of system registers.
	 * The calculated values are stored in the S-EL1 context before
	 * jumping to the code in S-EL0.
	 */
	secure_partition_setup();

	/*
	 * All SPM initialization done. Now register our init function with
	 * BL31 for deferred invocation
	 */
	bl31_register_bl32_init(&spm_init);
	VERBOSE("%s exit\n", __func__);
	return 0;
}

/*
 * Attributes are encoded using in a different format in the
 * SPM_MEMORY_ATTRIBUTES_SET SMC than in TF, where we use the mmap_attr_t
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

static int spm_memory_attributes_smc_handler(u_register_t page_address,
					u_register_t pages_count,
					u_register_t smc_attributes)
{
	NOTICE("Received SPM_MEMORY_ATTRIBUTES_SET SMC\n");

	uintptr_t base_va = (uintptr_t) page_address;
	size_t size = (size_t) (pages_count * PAGE_SIZE);
	int attributes = (int) smc_attributes;
	NOTICE("  Start address  : 0x%lx\n", base_va);
	NOTICE("  Number of pages: %i (%zi bytes)\n",
		(int) pages_count, size);
	NOTICE("  Attributes     : 0x%x\n", attributes);
	attributes = smc_attr_to_mmap_attr(attributes);
	NOTICE("  (Equivalent TF attributes: 0x%x)\n", attributes);

	return change_mem_attributes(secure_partition_xlat_ctx_handle,
				     base_va, size, attributes);
}


uint64_t spm_smc_handler(uint32_t smc_fid,
			 uint64_t x1,
			 uint64_t x2,
			 uint64_t x3,
			 uint64_t x4,
			 void *cookie,
			 void *handle,
			 uint64_t flags)
{
	cpu_context_t *ns_cpu_context;
	unsigned int ns, linear_id;

	linear_id = plat_my_core_pos();

	/* Determine which security state this SMC originated from */
	ns = is_caller_non_secure(flags);

	if (ns == SMC_FROM_SECURE) {
		switch (smc_fid) {
		case SP_EVENT_COMPLETE_AARCH64:
			assert(handle == cm_get_context(SECURE));
			cm_el1_sysregs_context_save(SECURE);
			spm_setup_next_eret_into_sel0(handle);

			if (SP_PSTATE_OFF ==
			    get_sp_pstate(sp_ctx[linear_id].flags)) {
				/*
				 * SPM reports completion. The SPM must have
				 * initiated the original request through a
				 * synchronous entry into the secure
				 * partition. Jump back to the original C
				 * runtime context.
				 */
				spm_synchronous_sp_exit(&sp_ctx[linear_id], x1);
				assert(0);
			}

			/*
			 * This is the result from the Secure partition of an
			 * earlier request. Copy the result into the non-secure
			 * context, save the secure state and return to the
			 * non-secure state.
			 */

			/* Get a reference to the non-secure context */
			ns_cpu_context = cm_get_context(NON_SECURE);
			assert(ns_cpu_context);

			/* Restore non-secure state */
			cm_el1_sysregs_context_restore(NON_SECURE);
			cm_set_next_eret_context(NON_SECURE);

			/* Return to normal world */
			SMC_RET1(ns_cpu_context, x1);

		case SP_MEM_ATTRIBUTES_SET_AARCH64:
			SMC_RET1(handle,
				 spm_memory_attributes_smc_handler(x1, x2, x3));
		default:
			break;
		}
	} else {
		switch (smc_fid) {
		case SP_COMMUNICATE_AARCH32:
		case SP_COMMUNICATE_AARCH64:

			/* Save the Normal world context */
			cm_el1_sysregs_context_save(NON_SECURE);

			/*
			 * Restore the secure world context and prepare for
			 * entry in S-EL0
			 */
			assert(&sp_ctx[linear_id].cpu_ctx ==
			       cm_get_context(SECURE));
			cm_el1_sysregs_context_restore(SECURE);
			cm_set_next_eret_context(SECURE);

			/*
			 * TODO: Print a warning if X2 is not NULL since that is
			 * the recommended approach
			 */
			SMC_RET4(&sp_ctx[linear_id].cpu_ctx,
				 smc_fid, x2, x3, plat_my_core_pos());

		default:
			break;
		}
	}
	/* TODO: Implement me */
	SMC_RET1(handle, SMC_UNK);
}
