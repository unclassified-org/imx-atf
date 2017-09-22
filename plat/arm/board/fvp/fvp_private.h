/*
 * Copyright (c) 2014-2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __FVP_PRIVATE_H__
#define __FVP_PRIVATE_H__

#include <gic_common.h>
#include <plat_arm.h>

/*******************************************************************************
 * Function and variable prototypes
 ******************************************************************************/

void fvp_config_setup(void);

void fvp_interconnect_init(void);
void fvp_interconnect_enable(void);
void fvp_interconnect_disable(void);

/* FVP exception setup */
void fvp_exception_init(void);

extern const interrupt_prop_t fvp_interrupts[];
extern const size_t fvp_interrupts_num;

#endif /* __FVP_PRIVATE_H__ */
