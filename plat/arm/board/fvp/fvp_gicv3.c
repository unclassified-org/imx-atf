/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arm_def.h>
#include <gicv3.h>
#include <plat_arm.h>
#include <platform.h>
#include <platform_def.h>
#include "fvp_private.h"

/* The GICv3 driver only needs to be initialized in EL3 */
static uintptr_t rdistif_base_addrs[PLATFORM_CORE_COUNT];

/*
 * MPIDR hashing function for translating MPIDRs read from GICR_TYPER register
 * to core position.
 *
 * Calculating core position is dependent on MPIDR_EL1.MT bit. However, affinity
 * values read from GICR_TYPER don't have an MT field. To reuse the same
 * translation used for CPUs, we insert MT bit read from the PE's MPIDR into
 * that read from GICR_TYPER.
 *
 * Assumptions:
 *
 *   - All CPUs implemented in the system have MPIDR_EL1.MT bit set;
 *   - No CPUs implemented in the system use affinity level 3.
 */
static unsigned int fvp_gicv3_mpidr_hash(u_register_t mpidr)
{
	mpidr |= (read_mpidr_el1() & MPIDR_MT_MASK);
	return plat_arm_calc_core_pos(mpidr);
}

/* GICv3 driver data to initialize driver with interrupt properties */
gicv3_driver_data_t fvp_gic_data = {
	.gicd_base = PLAT_ARM_GICD_BASE,
	.gicr_base = PLAT_ARM_GICR_BASE,
	.rdistif_num = PLATFORM_CORE_COUNT,
	.rdistif_base_addrs = rdistif_base_addrs,
	.mpidr_to_core_pos = fvp_gicv3_mpidr_hash
};

void plat_arm_gic_driver_init(void)
{
	/*
	 * The GICv3 driver is initialized in EL3 and does not need to be
	 * initialized again in SEL1. This is because the S-EL1 can use GIC
	 * system registers to manage interrupts and does not need GIC interface
	 * base addresses to be configured.
	 */
#if (defined(AARCH32) && defined(IMAGE_BL32)) || \
	(!defined(AARCH32) && defined(IMAGE_BL31))
	/* Populate FVP interrupt properties */
	fvp_gic_data.interrupt_props = fvp_interrupts;
	fvp_gic_data.interrupt_props_num = fvp_interrupts_num;

	gicv3_driver_init(&fvp_gic_data);
#endif
}
