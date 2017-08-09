/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>

	.globl	cactus_svc

func cactus_svc
	svc	#0
	ret
endfunc cactus_svc
