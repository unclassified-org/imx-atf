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

#ifndef __INTERRUPT_CLASS_H__
#define __INTERRUPT_CLASS_H__

#include <interrupt_mgmt.h>

/* PRIORITY_CLASS_RAS  		- priority 00-0F */
/* PRIORITY_UNALLOCATED		- priority 10-1F */
/* PRIORITY_CLASS_FW 		- priority 20-2F */
/* PRIORITY_CLASS_SEC_CSDE  	- priority 30-3F */
/* PRIORITY_CLASS_SEC_NSDE	- priority 40-4F */
#define PRIORITY_CLASS_SP	5	/* Priority 50-5F */
#define PRIORITY_CLASS_CSDE	6	/* Priority 0x60 - 0x6F */
#define PRIORITY_CLASS_NSDE	7	/* Priority 0x70 - 0x7F */

#define INTR_HANDLED		0
#define INTR_ERROR		1
#define INTR_DEFER_EOI		2

int el3_iclass_init();
int el3_iclass_register_handler(unsigned int class,
		interrupt_type_handler_t handler);

int el3_iclass_add_intr(unsigned int intr, unsigned int class);
int el3_iclass_remove_intr(unsigned int intr, unsigned int class);

unsigned int el3_iclass_mask(unsigned int class, unsigned int *flags);
void el3_iclass_unmask(unsigned int flags);

#endif /* __INTERRUPT_CLASS_H__ */
