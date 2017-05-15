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
#include <asm_macros.S>
#include <bl_common.h>
#include <context.h>

	/*
	 * Declare the exception vector table, enforcing it is aligned on a
	 * 2KB boundary, as required by the ARMv8 architecture.
	 * Use zero bytes as the fill value to be stored in the padding bytes
	 * so that it inserts illegal AArch64 instructions. This increases
	 * security, robustness and potentially facilitates debugging.
	 */
	.macro secure_partition_vector_base  label
	.section .secure_partition_vectors, "ax"
	.align 11, 0
	\label:
	.endm

	/*
	 * Create an entry in the exception vector table, enforcing it is
	 * aligned on a 128-byte boundary, as required by the ARMv8 architecture.
	 * Use zero bytes as the fill value to be stored in the padding bytes
	 * so that it inserts illegal AArch64 instructions. This increases
	 * security, robustness and potentially facilitates debugging.
	 */
	.macro secure_partition_vector_entry  label
	.section .secure_partition_vectors, "ax"
	.align 7, 0
	\label:
	.endm

	/*
	 * This macro verifies that the given vector doesn't exceed the
	 * architectural limit of 32 instructions. This is meant to be placed
	 * immediately after the last instruction in the vector. It takes the
	 * vector entry as the parameter
	 */
	.macro secure_partition_check_vector_size since
	  .if (. - \since) > (32 * 4)
	    .error "Vector exceeds 32 instructions"
	  .endif
	.endm

/* -----------------------------------------------------------------------------
 * Very simple stackless exception handlers used by the spm shim layer.
 * -----------------------------------------------------------------------------
 */
	.globl	secure_partition_exceptions

secure_partition_vector_base secure_partition_exceptions

	/* -----------------------------------------------------
	 * Current EL with SP0 : 0x0 - 0x200
	 * -----------------------------------------------------
	 */
secure_partition_vector_entry SynchronousExceptionSP0
	b	.
	secure_partition_check_vector_size SynchronousExceptionSP0

secure_partition_vector_entry IrqSP0
	b	.
	secure_partition_check_vector_size IrqSP0

secure_partition_vector_entry FiqSP0
	b	.
	secure_partition_check_vector_size FiqSP0

secure_partition_vector_entry SErrorSP0
	b	.
	secure_partition_check_vector_size SErrorSP0

	/* -----------------------------------------------------
	 * Current EL with SPx: 0x200 - 0x400
	 * -----------------------------------------------------
	 */
secure_partition_vector_entry SynchronousExceptionSPx
	b	.
	secure_partition_check_vector_size SynchronousExceptionSPx

secure_partition_vector_entry IrqSPx
	b	.
	secure_partition_check_vector_size IrqSPx

secure_partition_vector_entry FiqSPx
	b	.
	secure_partition_check_vector_size FiqSPx

secure_partition_vector_entry SErrorSPx
	b	.
	secure_partition_check_vector_size SErrorSPx

	/* -----------------------------------------------------
	 * Lower EL using AArch64 : 0x400 - 0x600. No exceptions
	 * are handled since secure_partition does not implement
	 * a lower EL
	 * -----------------------------------------------------
	 */
secure_partition_vector_entry SynchronousExceptionA64
	/* Enable the SError interrupt */
	msr	daifclr, #DAIF_ABT_BIT

	msr	tpidr_el1, x30
	mrs	x30, esr_el1
	ubfx	x30, x30, #ESR_EC_SHIFT, #ESR_EC_LENGTH

	cmp	x30, #EC_AARCH64_SVC
	b.eq 	do_smc

	cmp	x30, #EC_AARCH32_SVC
	b.ne	panic

	/* ---------------------------------------------
	 * Tell SPM that we are done initialising
	 * ---------------------------------------------
	 */
do_smc:
	mrs	x30, tpidr_el1
	smc	#0
	eret

panic:
	b	panic
	secure_partition_check_vector_size SynchronousExceptionA64

secure_partition_vector_entry IrqA64
	b	.
	secure_partition_check_vector_size IrqA64

secure_partition_vector_entry FiqA64
	b	.
	secure_partition_check_vector_size FiqA64

secure_partition_vector_entry SErrorA64
	b	.
	secure_partition_check_vector_size SErrorA64

	/* -----------------------------------------------------
	 * Lower EL using AArch32 : 0x600 - 0x800
	 * -----------------------------------------------------
	 */
secure_partition_vector_entry SynchronousExceptionA32
	b	.
	secure_partition_check_vector_size SynchronousExceptionA32

secure_partition_vector_entry IrqA32
	b	.
	secure_partition_check_vector_size IrqA32

secure_partition_vector_entry FiqA32
	b	.
	secure_partition_check_vector_size FiqA32

secure_partition_vector_entry SErrorA32
	b	.
	secure_partition_check_vector_size SErrorA32
