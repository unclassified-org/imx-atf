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

#ifndef __SDEI_H__
#define __SDEI_H__

#include <atomic.h>
#include <platform.h>
#include <spinlock.h>

/* Range 0xC4000020 - 0xC400003F reserved for SDE 64bit smc calls */
/* FIXME: consider reserving larger range */
#define SDEI_VERSION				0xC4000020
#define SDEI_EVENT_REGISTER			0xC4000021
#define SDEI_EVENT_ENABLE			0xC4000022
#define SDEI_EVENT_DISABLE			0xC4000023
#define SDEI_EVENT_CONTEXT			0xC4000024
#define SDEI_EVENT_COMPLETE			0xC4000025
#define SDEI_EVENT_COMPLETE_AND_RESUME		0xC4000026

#define SDEI_EVENT_UNREGISTER			0xC4000027
#define SDEI_EVENT_STATUS			0xC4000028
#define SDEI_EVENT_GET_INFO			0xC4000029
#define SDEI_EVENT_ROUTING_SET			0xC400002A
#define SDEI_PE_MASK				0xC400002B
#define SDEI_PE_UNMASK				0xC400002C

#define SDEI_INTERRUPT_BIND			0xC400002D
#define SDEI_INTERRUPT_RELEASE			0xC400002E
#define SDEI_EVENT_SIGNAL			0xC400002F
#define SDEI_FEATURES				0xC4000030
#define SDEI_PRIVATE_RESET			0xC4000031
#define SDEI_SHARED_RESET			0xC4000032

/* For debug */
#define SDEI_SHOW_DEBUG				0xC400003F

/* SDEI_EVENT_REGISTER flags */
/* TODO: move to bit definitions */
#define SDEI_REGF_RM_ANY	0
#define SDEI_REGF_RM_PE		1

/* SDEI_EVENT_COMPLETE status flags */
#define SDEI_EV_HANDLED		0
#define SDEI_EV_FAILED		1

/* SDE event status values in bit position */
#define SDEI_STATF_REGISTERED		0
#define SDEI_STATF_ENABLED		1
#define SDEI_STATF_RUNNING		2

#define SDEI_INFO_EV_TYPE		0
#define SDEI_INFO_EV_SIGNALABLE		1
#define SDEI_INFO_EV_CRITICAL		2
#define SDEI_INFO_EV_ROUTING_MODE	3
#define SDEI_INFO_EV_ROUTING_AFF	4

/* Internal: SDEI flag bit positions */
#define _SDEI_MAPF_DYNAMIC	1
#define _SDEI_MAPF_BOUND	2
#define _SDEI_MAPF_SIGNALABLE	3
#define _SDEI_MAPF_PRIVATE	4
#define _SDEI_MAPF_CRITICAL	5

/* SDEI flags */
#define SDEI_MAPF_DYNAMIC	(1 << _SDEI_MAPF_DYNAMIC)
#define SDEI_MAPF_BOUND		(2 << _SDEI_MAPF_BOUND)
#define SDEI_MAPF_SIGNALABLE	(3 << _SDEI_MAPF_SIGNALABLE)
#define SDEI_MAPF_PRIVATE	(4 << _SDEI_MAPF_PRIVATE)
#define SDEI_MAPF_CRITICAL	(5 << _SDEI_MAPF_CRITICAL)

#define SDEI_EVENT_MAP(_event, _intr, _flags) \
	{ \
		.ev_num = _event, \
		.intr = _intr, \
		.flags = _flags \
	}

#define SDEI_SHARED_EVENT(_event, _intr, _flags) \
	SDEI_EVENT_MAP(_event, _intr, _flags)

#define SDEI_PRIVATE_EVENT(_event, _intr, _flags) \
	SDEI_EVENT_MAP(_event, _intr, _flags | SDEI_MAPF_PRIVATE)

#define DECLARE_SDEI_MAP(_private, _shared) \
	const sdei_mapping_t sdei_global_mappings[] = { \
		{ \
			.map = _private, \
			.size = ARRAY_SIZE(_private) \
		}, \
		{ \
			.map = _shared, \
			.size = ARRAY_SIZE(_shared) \
		}, \
	}; \
	sdei_entry_t sdei_private_event_table \
		[PLATFORM_CORE_COUNT * ARRAY_SIZE(_private)]; \
	sdei_entry_t sdei_shared_event_table[ARRAY_SIZE(_shared)]

typedef void (*sdei_ep_t)(int event, unsigned long long arg);

/* Runtime data of SDEI event */
typedef struct sdei_entry {
	unsigned int state;	/* Event handler state */
	sdei_ep_t ep;		/* Entry point */
	uintptr_t arg;		/* Entry point argument */
	long flags;		/* Registration flags */
	long affinity;		/* Affinity of shared event */
	spinlock_t lock;	/* Per event lock */
} sdei_entry_t;

/* Mapping of SDEI events to interrupts, and associated data */
typedef struct sdei_ev_map {
	int ev_num;		/* Event number */
	int intr;		/* Physical intr number */
	unsigned int flags;	/* Mapping flags, see SDEI_MAPF_* */
	atomic_t usage_cnt;	/* Usage count */
} sdei_ev_map_t;

typedef struct sdei_mapping {
	sdei_ev_map_t *map;
	size_t size;
} sdei_mapping_t;

#endif /* __SDEI_H__ */
