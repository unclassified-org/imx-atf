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

#ifndef __SECURE_PARTITION_H__
#define __SECURE_PARTITION_H__

/* Handle on the Secure partition translation context */
extern xlat_ctx_t *secure_partition_xlat_ctx_handle;

/* Linker symbols */
extern uintptr_t __SECURE_PARTITION_XLAT_TABLES_START__;
extern uintptr_t __SECURE_PARTITION_XLAT_TABLES_END__;

/* Definitions */
#define SECURE_PARTITION_XLAT_TABLES_BASE	\
	(uintptr_t)(&__SECURE_PARTITION_XLAT_TABLES_START__)
#define SECURE_PARTITION_XLAT_TABLES_END	\
	(uintptr_t)(&__SECURE_PARTITION_XLAT_TABLES_END__)
#define SECURE_PARTITION_XLAT_TABLES_SIZE	\
	(SECURE_PARTITION_XLAT_TABLES_END - SECURE_PARTITION_XLAT_TABLES_BASE)

/*
 * Flags used by the secure_partition_mp_info structure to describe the
 * characteristics of a cpu. Only a single flag to indicate the primary cpu is
 * defined at the moment.
 */
#define MP_INFO_FLAG_PRIMARY_CPU	0x00000001

/*
 * This structure is used to provide information required to initialise a S-EL0
 * partition.
 */
typedef struct secure_partition_mp_info {
	unsigned long mpidr;
	unsigned int  linear_id;
	unsigned int  flags;
} secure_partition_mp_info_t;

typedef struct secure_partition_boot_info {
	param_header_t 			h;
	unsigned long			sp_mem_base;
	unsigned long			sp_mem_limit;
	unsigned long			sp_image_base;
	unsigned long 			sp_stack_base;
	unsigned long 			sp_heap_base;
	unsigned long 			sp_ns_comm_buf_base;
	unsigned long 			sp_shared_buf_base;
	unsigned int 			sp_image_size;
	unsigned int 			sp_pcpu_stack_size;
	unsigned int 			sp_heap_size;
	unsigned int 			sp_ns_comm_buf_size;
	unsigned int 			sp_pcpu_shared_buf_size;
	unsigned int 			num_sp_mem_regions;
	unsigned int 			num_cpus;
	secure_partition_mp_info_t 	*mp_info;
} secure_partition_boot_info_t;

typedef struct secure_partition_warm_boot_info {
	param_header_t 			h;
	unsigned long 			sp_stack_base;
	unsigned long 			sp_shared_buf_base;
	unsigned int 			sp_pcpu_stack_size;
	unsigned int 			sp_pcpu_shared_buf_size;
	secure_partition_mp_info_t 	mp_info;
} sp_warm_boot_info_t;

/* General setup functions for secure partitions context. */

void secure_partition_setup(void);
void secure_partition_prepare_warm_boot_context(void);
void secure_partition_prepare_context(void);

#endif /* __SECURE_PARTITION_H__ */
