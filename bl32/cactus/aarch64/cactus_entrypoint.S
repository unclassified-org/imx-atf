/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>
#include <spm_svc.h>

	.globl	cactus_entrypoint


func cactus_entrypoint
	/*
	 * Allocate a stack.
	 */
	bl	plat_set_my_stack

	/* And do the rest in C code */
	bl	cactus_main

	/* Tell SPM that we are done initialising */
	mov	x1, #0
	mov_imm	x0, SP_EVENT_COMPLETE_AARCH64
	svc	#0

	/* Loop forever */
cactus_entrypoint_panic:
	b	cactus_entrypoint_panic
endfunc cactus_entrypoint


