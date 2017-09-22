/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Exception configuration for FVP */

#include <assert.h>
#include <gic_common.h>
#include <gicv2.h>
#include <gicv3.h>

/* Select interrupt group for FVP platform based on the GIC driver chosen */
#if FVP_USE_GIC_DRIVER == FVP_GICV3

/* For GICv3, use separate classes for both groups */
# define FVP_S_EL1_GRP		INTR_GROUP1S
# define FVP_EL3_GRP		INTR_GROUP0

#else

/* For GICv2, both classes fall to Group 0 */
# define FVP_S_EL1_GRP		GICV2_INTR_GROUP0
# define FVP_EL3_GRP		GICV2_INTR_GROUP0

#endif

/* FVP interrupt properties */
const interrupt_prop_t fvp_interrupts[] = {
	/* Platform interrupts: SEL1 */
	PLAT_ARM_G1S_IRQ_PROPS(GIC_HIGHEST_SEC_PRIORITY, FVP_S_EL1_GRP),

	/* Platform interrupts: EL3 */
	PLAT_ARM_G0_IRQ_PROPS(GIC_HIGHEST_SEC_PRIORITY, FVP_EL3_GRP),
};

const size_t fvp_interrupts_num = ARRAY_SIZE(fvp_interrupts);
