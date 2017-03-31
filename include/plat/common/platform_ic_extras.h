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

#ifndef __PLATFORM_IC_EXTRAS_H__
#define __PLATFORM_IC_EXTRAS_H__

/* Different interrupt routing models */
#define INTR_ROUTING_MODE_PE		0
#define INTR_ROUTING_MODE_1_OF_N	1
#define INTR_ROUTING_MODE_TARGETS	2

void plat_ic_disable_interrupt(unsigned int id);
void plat_ic_enable_interrupt(unsigned int id);
void plat_ic_set_interrupt_group(unsigned int id, unsigned  int group);
void plat_ic_set_interrupt_priority(unsigned int id, unsigned int priority);
void plat_ic_set_interrupt_priority_mask(unsigned int pri);
unsigned int plat_ic_get_interrupt_priority_mask(void);
void plat_ic_set_interrupt_clear_pending(unsigned int id);
unsigned int plat_ic_get_interrupt_active(unsigned int id);
void plat_ic_clear_interrupt_pending(unsigned int id);
void plat_ic_set_interrupt_routing(unsigned int id, unsigned int routing_mode,
						unsigned long long mpidr);
unsigned int plat_ic_get_running_priority(void);

#endif /* __PLATFORM_IC_EXTRAS_H__ */
