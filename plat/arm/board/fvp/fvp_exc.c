/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Exception configuration for FVP */

#include <assert.h>
#include <exception_mgmt.h>
#include <gic_common.h>
#include <gicv2.h>
#include <gicv3.h>
#include <sdei.h>

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

/* FVP uses only 3 upper bits of interrupt priority */
#define FVP_PRI_BITS		3

/* Exception priorities for EL3 dispatchers */
#define SDEI_CRITICAL_PRIORITY	0x60
#define SDEI_NORMAL_PRIORITY	0x70

/* FVP interrupt properties */
const interrupt_prop_t fvp_interrupts[] = {
	/* Platform interrupts: SEL1 */
	PLAT_ARM_G1S_IRQ_PROPS(GIC_HIGHEST_SEC_PRIORITY, FVP_S_EL1_GRP),

	/* Platform interrupts: EL3 */
	PLAT_ARM_G0_IRQ_PROPS(GIC_HIGHEST_SEC_PRIORITY, FVP_EL3_GRP),

#if SDEI_SUPPORT
	/* SDEI interrupts */
	INTR_PROP_DESC(8, SDEI_NORMAL_PRIORITY, FVP_EL3_GRP, INTR_CFG_EDGE),
	INTR_PROP_DESC(23, SDEI_NORMAL_PRIORITY, FVP_EL3_GRP, INTR_CFG_LEVEL),
	INTR_PROP_DESC(35, SDEI_NORMAL_PRIORITY, FVP_EL3_GRP, INTR_CFG_LEVEL),
#endif
};

const size_t fvp_interrupts_num = ARRAY_SIZE(fvp_interrupts);

#if SDEI_SUPPORT
/* XXX: Cleanup the list of events on FVP */
/*
 * Mapping table provides mapping of event to interrupts and the properties of
 * the map like private, dynamic event etc. The mapping table for private and
 * shared are kept separate to simplify implementation. Each private/shared map
 * will have a corresponding event entry in the same array offset in
 * private/shared respectively. They are again separated as it allows to avoid
 * false sharing and better locking.
 */
static sdei_ev_map_t fvp_private_sdei[] = {
	/* Private event mappings */
	SDEI_PRIVATE_EVENT(0, 8, SDEI_MAPF_SIGNALABLE),

	/* PMU interrupt */
	SDEI_PRIVATE_EVENT(8, 23, SDEI_MAPF_BOUND),

	/* Dynamic private events */
	SDEI_PRIVATE_EVENT(100, 0, SDEI_MAPF_DYNAMIC),
	SDEI_PRIVATE_EVENT(101, 0, SDEI_MAPF_DYNAMIC)
};

/* Shared event mappings */
static sdei_ev_map_t fvp_shared_sdei[] = {
	/* SP804 Timer 0 */
	SDEI_SHARED_EVENT(804, 0, SDEI_MAPF_DYNAMIC),

	/* SP804 Timer 1 */
	SDEI_SHARED_EVENT(1804, 35, SDEI_MAPF_BOUND),

	/* Dynamic shared events */
	SDEI_SHARED_EVENT(3000, 0, SDEI_MAPF_DYNAMIC),
	SDEI_SHARED_EVENT(3001, 0, SDEI_MAPF_DYNAMIC)
};

/* Export FVP SDEI events */
DECLARE_SDEI_MAP(fvp_private_sdei, fvp_shared_sdei);
#endif

/* Initialize FVP exceptions */
void fvp_exception_init(void)
{
#if SDEI_SUPPORT
	/* Initialize SDEI */
	if (sdei_init(SDEI_CRITICAL_PRIORITY, SDEI_NORMAL_PRIORITY)) {
		panic();
	}
#endif
}

/*
 * Enumeration of FVP exceptions to be handled in EL3.
 */
exc_pri_desc_t fvp_exceptions[] = {
#if SDEI_SUPPORT
	/* Critical priority SDEI descriptor */
	EXC_INSTALL_DESC(FVP_PRI_BITS, SDEI_CRITICAL_PRIORITY,
			sdei_intr_handler),

	/* Normal priority SDEI descriptor */
	EXC_INSTALL_DESC(FVP_PRI_BITS, SDEI_NORMAL_PRIORITY, sdei_intr_handler),
#endif
};

/* Plug in FVP exceptions to Exception Handling Framework. */
DECLARE_EXCEPTIONS(fvp_exceptions, ARRAY_SIZE(fvp_exceptions), FVP_PRI_BITS);
