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

#ifndef __SDEI_SVC_H__
#define __SDEI_SVC_H__

#define SDEI_NUM_CALLS			32

/* The macros below are used to identify SDEI calls from the SMC function ID */
#define SDEI_FID_MASK			0xFFE0UL
#define SDEI_FID_VALUE			0x20UL
#define is_sdei_fid(_fid) \
	(((_fid) & SDEI_FID_MASK) == SDEI_FID_VALUE)

/* Handler to be called to handle SDEI smc calls */
uint64_t sdei_smc_handler(uint32_t smc_fid,
			  uint64_t x1,
			  uint64_t x2,
			  uint64_t x3,
			  uint64_t x4,
			  void *cookie,
			  void *handle,
			  uint64_t flags);

int sdei_early_setup(void);
#if 0
typedef void (*sdei_event_complete_callback)(int id, int status, long arg);


int sdei_register_interrupt_handling(uint64_t fiq_entry,
					sdei_event_complete_callback cb);

/* Fiq handler which handles sde */
uint64_t sdei_el3_fiq_handler(uint32_t id,
			    uint32_t flags, void *handle, void *cookie);

/* For SPD, to indicate the S-EL1 FIQ is completed by SP */
void sdei_sfiq_complete(void);

/* To enable/disable S-EL1 fiq (SFIQ) interrupts from SPD */
void sdei_local_sfiq_enable(void);
void sdei_local_sfiq_disable(void);

/* To get S-EL1 fiq interrupts mask from SPD */
unsigned long get_sdei_local_sfiq_mask(void);

/* To save/restore S-EL1 fiq interrupts from SPD onto flags */
#define sdei_local_sfiq_save(flags) { \
	flags = get_sdei_local_sfiq_getmask(); \
	sdei_local_sfiq_disable(); \
	}
void sdei_local_sfiq_restore(unsigned long flags);

/* SDEI PM handlers that need to be called from SPD */
void sdei_cpu_off_handler(void);
void sdei_cpu_on_finish_handler(void);
void sdei_cpu_suspend_handler(void);
void sdei_cpu_suspend_finish_handler(void);
#endif

#endif /* __SDEI_SVC_H__ */
