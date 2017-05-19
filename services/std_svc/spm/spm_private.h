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

#ifndef __SPM_PRIVATE_H__
#define __SPM_PRIVATE_H__

#include <context.h>

/*******************************************************************************
 * Secure Partition PM state information e.g. partition is suspended,
 * uninitialised etc and macros to access the state information in the per-cpu
 * flags
 ******************************************************************************/
#define SP_PSTATE_OFF		0
#define SP_PSTATE_ON		1
#define SP_PSTATE_SUSPEND	2
#define SP_PSTATE_SHIFT		0
#define SP_PSTATE_MASK		0x3
#define get_sp_pstate(flags)	(((flags) >> SP_PSTATE_SHIFT) & SP_PSTATE_MASK)
#define clr_sp_pstate(flags)	((flags) &= ~(SP_PSTATE_MASK <<		\
					      SP_PSTATE_SHIFT))
#define set_sp_pstate(fl, pst)	do {					\
				    clr_sp_pstate(fl);			\
				    (fl) |= ((pst) & SP_PSTATE_MASK) <<	\
					    SP_PSTATE_SHIFT;		\
				} while (0);

/*******************************************************************************
 * Constants that allow assembler code to preserve callee-saved registers of the
 * C runtime context while performing a security state switch.
 ******************************************************************************/
#define SPM_C_RT_CTX_X19		0x0
#define SPM_C_RT_CTX_X20		0x8
#define SPM_C_RT_CTX_X21		0x10
#define SPM_C_RT_CTX_X22		0x18
#define SPM_C_RT_CTX_X23		0x20
#define SPM_C_RT_CTX_X24		0x28
#define SPM_C_RT_CTX_X25		0x30
#define SPM_C_RT_CTX_X26		0x38
#define SPM_C_RT_CTX_X27		0x40
#define SPM_C_RT_CTX_X28		0x48
#define SPM_C_RT_CTX_X29		0x50
#define SPM_C_RT_CTX_X30		0x58
#define SPM_C_RT_CTX_SIZE		0x60
#define SPM_C_RT_CTX_ENTRIES		(SPM_C_RT_CTX_SIZE >> DWORD_SHIFT)


#ifndef __ASSEMBLY__

#include <stdint.h>

typedef struct spm_context {
	uint64_t c_rt_ctx;
	uint32_t flags;
	cpu_context_t cpu_ctx;
} spm_context_t;

uint64_t spm_secure_partition_enter(uint64_t *c_rt_ctx);
void __dead2 spm_secure_partition_exit(uint64_t c_rt_ctx, uint64_t ret);

#endif /* __ASSEMBLY__ */

#endif /* __SPM_PRIVATE_H__ */
