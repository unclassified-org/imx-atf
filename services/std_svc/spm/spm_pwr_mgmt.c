/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <assert.h>
#include <bl_common.h>
#include <context_mgmt.h>
#include <debug.h>
#include <platform.h>
#include <secure_partition.h>
#include "spm_private.h"

/*******************************************************************************
 * This cpu is being turned off. Allow the secure partition to perform any
 * actions needed.
 ******************************************************************************/
static int spm_cpu_off_handler(unsigned long unused)
{
	unsigned int linear_id = plat_my_core_pos();
	secure_partition_context_t *sp_ctx_ptr = &sp_ctx[linear_id];

	assert(get_sp_pstate(sp_ctx_ptr->flags) == SP_PSTATE_ON);

	/*
	 * Set the partition state to off for a fresh start when this cpu is
	 * turned on subsequently.
	 */
	set_sp_pstate(sp_ctx_ptr->flags, SP_PSTATE_OFF);

	return 0;
}

/*******************************************************************************
 * This cpu has been turned on. Enter the partition to initialise it. Entry in
 * S-EL0 is done after initialising minimal architectural state that guarantees
 * safe execution.
 ******************************************************************************/
static void spm_cpu_on_finish_handler(unsigned long unused)
{
	int rc = 0;
	unsigned int linear_id = plat_my_core_pos();
	secure_partition_context_t *sp_ctx_ptr = &sp_ctx[linear_id];
	entry_point_info_t sp_ep_info;

	assert(get_sp_pstate(sp_ctx_ptr->flags) == SP_PSTATE_OFF);

	/* Initialise the entry point information for this secondary CPU */
	spm_init_sp_ep_state(&sp_ep_info, 
			     warm_boot_entry_point, 
			     &sp_ctx[linear_id]);

	/*
	 * Initialise the common context and then overlay the S-EL0 specific
	 * context on top of it.
	 */
	cm_init_my_context(&sp_ep_info);

	secure_partition_prepare_warm_boot_context();

	/* Enter the Secure partition */
	rc = spm_synchronous_sp_entry(&sp_ctx[linear_id]);
	if (rc)
		panic();

	/* Mark the partition as being ON on this CPU */
	set_sp_pstate(sp_ctx[linear_id].flags, SP_PSTATE_ON);
}

/*******************************************************************************
 * This cpu is being suspended. Save any secure partition state.
 * - Memory state will be automatically preserved as the caches will be flushed.
 * - System register state for a partition has been saved in its context
 *   information.
 * - Device state will need to be saved but at the moment there are no devices
 *   local to this cpu that we care about.
 ******************************************************************************/
static void spm_cpu_suspend_handler(unsigned long max_off_pwrlvl)
{
	unsigned int linear_id = plat_my_core_pos();
	secure_partition_context_t *sp_ctx_ptr = &sp_ctx[linear_id];

	assert(get_sp_pstate(sp_ctx_ptr->flags) == SP_PSTATE_ON);

	/* Update the context to reflect the state the partition is in */
	set_sp_pstate(sp_ctx_ptr->flags, SP_PSTATE_SUSPEND);
}


/*******************************************************************************
 * This cpu has been resumed from suspend. Restore any secure partition state.
 * - Memory state has automatically been preserved as caches were flushed.
 * - System register state for a partition was saved in its context information
 *   and will be restored upon the next ERET into the partition.
 * - There is no device state to worry about right now.
 ******************************************************************************/
static void spm_cpu_suspend_finish_handler(unsigned long max_off_pwrlvl)
{
	unsigned int linear_id = plat_my_core_pos();
	secure_partition_context_t *sp_ctx_ptr = &sp_ctx[linear_id];

	assert(get_sp_pstate(sp_ctx_ptr->flags) == SP_PSTATE_SUSPEND);

	/* Update the context to reflect the state the partition is in */
	set_sp_pstate(sp_ctx_ptr->flags, SP_PSTATE_SUSPEND);
}

/*******************************************************************************
 * Structure populated by the Secure Partition Manager to be given a chance to
 * perform any partition specific bookkeeping before PSCI executes a power
 * management operation.
 ******************************************************************************/
const spd_pm_ops_t spm_pm = {
	.svc_on = NULL,
	.svc_off = spm_cpu_off_handler,
	.svc_suspend = spm_cpu_suspend_handler,
	.svc_on_finish = spm_cpu_on_finish_handler,
	.svc_suspend_finish = spm_cpu_suspend_finish_handler,
	.svc_migrate = NULL,
	.svc_migrate_info = NULL,
	.svc_system_off = NULL,
	.svc_system_reset = NULL
};
