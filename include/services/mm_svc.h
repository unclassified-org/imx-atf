/*
 * Copyright (c) 2014-2017, ARM Limited and Contributors. All rights reserved.
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

#ifndef __MM_SVC_H__
#define __MM_SVC_H__

/* The macros below are used to identify MM calls from the SMC function ID */
#define MM_FID_MASK			0xffffu
#define MM_FID_MIN_VALUE		0x40
#define MM_FID_MAX_VALUE		0x5f
#define is_mm_fid(_fid)						\
	((((_fid) & MM_FID_MASK) >= MM_FID_MIN_VALUE) &&	\
	 (((_fid) & MM_FID_MASK) <= MM_FID_MAX_VALUE))

/* SMC ID for MM_MEMORY_ATTRIBUTES_SET (SMC64 version) */
#define MM_MEMORY_ATTRIBUTES_SET	0xC4000047

/*
 * SMC ID used by the S-EL0 image to indicate it has finished initialising
 * after a cold boot.
 * XXX: not sure what ID value we're supposed to use, I can't find it in the
 * MM documents...
 */
#define MM_INIT_COMPLETE_AARCH64	0xC4000040

#ifndef __ASSEMBLY__

int32_t mmd_setup(void);

uint64_t mmd_smc_handler(uint32_t smc_fid,
			 uint64_t x1,
			 uint64_t x2,
			 uint64_t x3,
			 uint64_t x4,
			 void *cookie,
			 void *handle,
			 uint64_t flags);

#endif /* __ASSEMBLY__ */

#endif /* __MM_SVC_H__ */
