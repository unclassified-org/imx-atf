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

#ifndef __SPM_SVC_H__
#define __SPM_SVC_H__

/* The macros below are used to identify MM calls from the SMC function ID */
#define SPM_FID_MASK			0xffffu
#define SPM_FID_MIN_VALUE		0x40
#define SPM_FID_MAX_VALUE		0x7f
#define is_spm_fid(_fid)						\
	((((_fid) & SPM_FID_MASK) >= SPM_FID_MIN_VALUE) &&	\
	 (((_fid) & SPM_FID_MASK) <= SPM_FID_MAX_VALUE))

/*
 * SMC IDs defined in [1] for accessing services implemented by the Secure
 * Partitions Manager from the partitions. These services enable a partition to
 * handle delegated events and request privileged operations from the manager.
 * [1] DEN0061A_ARM_XX_XX_Specification.pdf
 */
#define SPM_INTERFACE_VERSION_AARCH64	0xC4000060
#define SP_EVENT_COMPLETE_AARCH64	0xC4000061
#define SP_MEM_ATTRIBUTES_GET_AARCH64	0xC4000064
#define SP_MEM_ATTRIBUTES_SET_AARCH64	0xC4000065

/*
 * SMC IDs defined in [1] for accessing secure partition services from the
 * normal world. These FIDs occupy the range 0x40 - 0x5f
 * [1] DEN0060A_ARM_MM_Interface_Specification.pdf
 */
#define SP_VERSION_AARCH64		0xC4000040
#define SP_VERSION_AARCH32		0x84000040

#define SP_COMMUNICATE_AARCH64		0xC4000041
#define SP_COMMUNICATE_AARCH32		0x84000041

#ifndef __ASSEMBLY__

int32_t spm_setup(void);

uint64_t spm_smc_handler(uint32_t smc_fid,
			 uint64_t x1,
			 uint64_t x2,
			 uint64_t x3,
			 uint64_t x4,
			 void *cookie,
			 void *handle,
			 uint64_t flags);

#endif /* __ASSEMBLY__ */

#endif /* __SPM_SVC_H__ */
