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

#ifndef __SDE_PRIVATE_H__
#define __SDE_PRIVATE_H__

#include <errno.h>
#include <platform.h>
#include <spinlock.h>
#include <sdei.h>
#include <types.h>

/* General helpers */
#define this_cpu() 	plat_my_core_pos()
#define OTHER_SS(_ss) 	(~(_ss) & NON_SECURE)

/* Bit manipulation helpers */
#define set_bit(_v, _b)		(_v) |= (1 << (_b))
#define clr_bit(_v, _b) 	(_v) &= ~(1 << (_b))
#define get_bit(_v, _b) 	(((_v) >> (_b)) & 1)

/* Flag manipulation helpers */
#define set_sde_flag(_s, _b) 	set_bit((_s)->flags, _b)
#define clr_sde_flag(_s, _b) 	clr_bit((_s)->flags, _b)
#define get_sde_flag(_s, _b) 	get_bit((_s)->flags, _b)

#define is_spi(_n) 	((_n) >= 32)

/* FIXME: move to / get from approp. header files. */
#define	SMC_EINVAL	2
#define	SMC_EDENY	3
#define	SMC_EPEND	5
#define	SMC_ENOMEM	10

#define is_event_private(_map) \
		get_bit((_map)->flags, _SDEI_MAPF_PRIVATE)

#define is_event_critical(_map) \
		get_bit((_map)->flags, _SDEI_MAPF_CRITICAL)

#define is_event_signalable(_map) \
		get_bit((_map)->flags, _SDEI_MAPF_SIGNALABLE)

#define is_map_dynamic(_map) \
		get_bit((_map)->flags, _SDEI_MAPF_DYNAMIC)

#define is_map_bound(_map) \
		get_bit((_map)->flags, _SDEI_MAPF_BOUND)

#define set_map_bound(_map) \
		set_bit((_map)->flags, _SDEI_MAPF_BOUND)

#define clr_map_bound(_map) \
		clr_bit((_map)->flags, _SDEI_MAPF_BOUND)

/* sde_entry.state macros */
#define get_ev_state_val(_e)	((_e)->state)
#define get_ev_state(_e, _s)	get_bit((_e)->state, SDEI_STATF_##_s)
#define set_ev_state(_e, _s)	set_bit((_e)->state, SDEI_STATF_##_s)
#define clr_ev_state(_e, _s)	clr_bit((_e)->state, SDEI_STATF_##_s)

#define SDEI_MAP_IDX_PRIV	0
#define SDEI_MAP_IDX_SHRD	1
#define SDEI_MAP_IDX_MAX        2

#define for_each_mapping(_i, _mapping) \
	for (_i = 0, _mapping = &sdei_global_mappings[i]; _i < SDEI_MAP_IDX_MAX; \
			_i++, _mapping = &sdei_global_mappings[i])

#define iterate_mapping(_mapping, _i, _map) \
	for (_map = (_mapping)->map, _i = 0;  _i < (_mapping)->size; _i++, _map++)

#define for_each_map_type(_shared, _i, _map) \
	iterate_mapping( \
		(_shared ? &sdei_global_mappings[SDEI_MAP_IDX_SHRD] : \
		 &sdei_global_mappings[SDEI_MAP_IDX_PRIV]), \
		_i, _map)

#define for_each_shared_map(_i, _map)	for_each_map_type(1, _i, _map)
#define for_each_private_map(_i, _map)	for_each_map_type(0, _i, _map)

#define SDEI_LOG(...)	INFO("SDEI: " __VA_ARGS__)

extern const sdei_mapping_t sdei_global_mappings[];
extern sdei_entry_t sdei_private_event_table[];
extern sdei_entry_t sdei_shared_event_table[];

int sdei_early_setup(void);
void init_sdei_state(void);

sdei_ev_map_t *find_event_map_by_intr(uint32_t intr_num, int shared);
sdei_ev_map_t *find_event_map(uint32_t ev_num);
sdei_entry_t *get_event_entry(sdei_ev_map_t *map);

void sdei_event_lock(sdei_entry_t *se, sdei_ev_map_t *map);
void sdei_event_unlock(sdei_entry_t *se, sdei_ev_map_t *map);
void sdei_ic_unregister(sdei_ev_map_t *map);
void unset_sdei_entry(sdei_entry_t *se);
int sdei_event_context(void *handle, unsigned int param);
int sdei_event_complete(int resume, uint64_t arg);
uint64_t sdei_intr_handler(uint32_t intr, uint32_t flags, void *handle,
		void *cookie);

int sdei_pe_unmask(void);
int sdei_pe_mask(void);

#endif /* __SDE_PRIVATE_H__ */
