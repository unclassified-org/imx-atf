/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SDEI_PRIVATE_H__
#define __SDEI_PRIVATE_H__

#include <errno.h>
#include <platform.h>
#include <spinlock.h>
#include <sdei.h>
#include <types.h>
#include <utils_def.h>

#define SDEI_DEBUG	0

#if SDEI_DEBUG
# define SDEI_LOG(...)	INFO("SDEI: " __VA_ARGS__)
#else
# define SDEI_LOG(...)
#endif

/* General helpers */
#define OTHER_SS(_ss) 	(~(_ss) & NON_SECURE)

/* Bit manipulation helpers */
#define _set_bit(_v, _b)	(_v) |= BIT(_b)
#define _clr_bit(_v, _b) 	(_v) &= ~(BIT(_b))
#define _get_bit(_v, _b) 	(((_v) >> (_b)) & 1)

#define is_sgi(_n)		plat_ic_is_sgi(_n)
#define is_secure_sgi(_n) \
	(plat_ic_is_sgi(_n) && (plat_ic_get_interrupt_type(_n) == INTR_TYPE_EL3))
#define is_spi(_n)		plat_ic_is_spi(_n)

/* SDE event status values in bit position */
#define SDEI_STATF_REGISTERED		0
#define SDEI_STATF_ENABLED		1
#define SDEI_STATF_RUNNING		2

/* 'info' parameters for SDEI_EVENT_GET_INFO SMC */
#define SDEI_INFO_EV_TYPE		0
#define SDEI_INFO_EV_SIGNALED		1
#define SDEI_INFO_EV_PRIORITY		2
#define SDEI_INFO_EV_ROUTING_MODE	3
#define SDEI_INFO_EV_ROUTING_AFF	4

/* SDEI SMC error codes */
#define	SDEI_EINVAL	2
#define	SDEI_EDENY	3
#define	SDEI_EPEND	5
#define	SDEI_ENOMEM	10

#define is_event_private(_map) \
		_get_bit((_map)->flags, _SDEI_MAPF_PRIVATE_SHIFT)

#define is_event_critical(_map) \
		_get_bit((_map)->flags, _SDEI_MAPF_CRITICAL_SHIFT)

#define is_event_signalable(_map) \
		_get_bit((_map)->flags, _SDEI_MAPF_SIGNALABLE_SHIFT)

#define is_map_dynamic(_map) \
		_get_bit((_map)->flags, _SDEI_MAPF_DYNAMIC_SHIFT)

/*
 * Static events always return true and dynamic events
 * return the actual state. So this can be safely used
 * to see if an event is bound (static or dynamic).
 */
#define is_map_bound(_map) \
		_get_bit((_map)->flags, _SDEI_MAPF_BOUND_SHIFT)

#define set_map_bound(_map) \
		_set_bit((_map)->flags, _SDEI_MAPF_BOUND_SHIFT)

#define clr_map_bound(_map) \
		_clr_bit((_map)->flags, _SDEI_MAPF_BOUND_SHIFT)

/* sde_entry.state macros */
#define get_ev_state_val(_e)	((_e)->state)
#define get_ev_state(_e, _s)	_get_bit((_e)->state, SDEI_STATF_##_s)
#define set_ev_state(_e, _s)	_set_bit((_e)->state, SDEI_STATF_##_s)
#define clr_ev_state(_e, _s)	_clr_bit((_e)->state, SDEI_STATF_##_s)

/* Indices of private and shared mappings */
#define SDEI_MAP_IDX_PRIV	0
#define SDEI_MAP_IDX_SHRD	1
#define SDEI_MAP_IDX_MAX        2

#define SDEI_PRIVATE_MAPPING()	(&sdei_global_mappings[SDEI_MAP_IDX_PRIV])
#define SDEI_SHARED_MAPPING()	(&sdei_global_mappings[SDEI_MAP_IDX_SHRD])

#define for_each_mapping_type(_i, _mapping) \
	for (_i = 0, _mapping = &sdei_global_mappings[i]; \
			_i < SDEI_MAP_IDX_MAX; \
			_i++, _mapping = &sdei_global_mappings[i])

#define iterate_mapping(_mapping, _i, _map) \
	for (_map = (_mapping)->map, _i = 0; \
			_i < (_mapping)->num_maps; \
			_i++, _map++)

#define for_each_private_map(_i, _map) \
	iterate_mapping(SDEI_PRIVATE_MAPPING(), _i, _map)

#define for_each_shared_map(_i, _map) \
	iterate_mapping(SDEI_SHARED_MAPPING(), _i, _map)

/* SDEI_FEATURES */
#define SDEI_FEATURE_BIND_SLOTS		0
#define BIND_SLOTS_MASK			0xffff
#define FEATURES_SHARED_SLOTS_SHIFT	16
#define FEATURES_PRIVATE_SLOTS_SHIFT	0
#define FEATURE_BIND_SLOTS(_priv, _shrd) \
	((((_priv) & BIND_SLOTS_MASK) << FEATURES_PRIVATE_SLOTS_SHIFT) | \
	 (((_shrd) & BIND_SLOTS_MASK) << FEATURES_SHARED_SLOTS_SHIFT))

extern unsigned int sdei_normal_pri;
extern unsigned int sdei_critical_pri;

extern const sdei_mapping_t sdei_global_mappings[];
extern sdei_entry_t sdei_private_event_table[];
extern sdei_entry_t sdei_shared_event_table[];

void init_sdei_state(void);

sdei_ev_map_t *find_event_map_by_intr(int intr_num, int shared);
sdei_ev_map_t *find_event_map(int ev_num);
sdei_entry_t *get_event_entry(sdei_ev_map_t *map);

void sdei_event_lock(sdei_entry_t *se, sdei_ev_map_t *map);
void sdei_event_unlock(sdei_entry_t *se, sdei_ev_map_t *map);
void sdei_ic_unregister(sdei_ev_map_t *map);
void unset_sdei_entry(sdei_entry_t *se);
int sdei_event_context(void *handle, unsigned int param);
int sdei_event_complete(int resume, uint64_t arg);
void sdei_map_lock(sdei_ev_map_t *map);
void sdei_map_unlock(sdei_ev_map_t *map);

int sdei_pe_unmask(void);
int sdei_pe_mask(void);
int mask_this_pe(void);
int unmask_this_pe(void);

#endif /* __SDEI_PRIVATE_H__ */
