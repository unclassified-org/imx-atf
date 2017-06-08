/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SDEI_H__
#define __SDEI_H__

#include <exception_mgmt.h>
#include <platform.h>
#include <spinlock.h>
#include <utils_def.h>

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
#define SDEI_REGF_RM_ANY	0
#define SDEI_REGF_RM_PE		1

/* SDEI_EVENT_COMPLETE status flags */
#define SDEI_EV_HANDLED		0
#define SDEI_EV_FAILED		1

/* SDE event status values in bit position */
#define SDEI_STATF_REGISTERED		0
#define SDEI_STATF_ENABLED		1
#define SDEI_STATF_RUNNING		2

/* 'info' parameter to SDEI_EVENT_GET_INFO SMC */
#define SDEI_INFO_EV_TYPE		0
#define SDEI_INFO_EV_SIGNALED		1
#define SDEI_INFO_EV_PRIORITY		2
#define SDEI_INFO_EV_ROUTING_MODE	3
#define SDEI_INFO_EV_ROUTING_AFF	4

/* Internal: SDEI flag bit positions */
#define _SDEI_MAPF_DYNAMIC_SHIFT	1
#define _SDEI_MAPF_BOUND_SHIFT		2
#define _SDEI_MAPF_SIGNALABLE_SHIFT	3
#define _SDEI_MAPF_PRIVATE_SHIFT	4
#define _SDEI_MAPF_CRITICAL_SHIFT	5

/* SDEI flags */
#define SDEI_MAPF_DYNAMIC	BIT(_SDEI_MAPF_DYNAMIC_SHIFT)
#define SDEI_MAPF_BOUND		BIT(_SDEI_MAPF_BOUND_SHIFT)
#define SDEI_MAPF_SIGNALABLE	BIT(_SDEI_MAPF_SIGNALABLE_SHIFT)
#define SDEI_MAPF_PRIVATE	BIT(_SDEI_MAPF_PRIVATE_SHIFT)
#define SDEI_MAPF_CRITICAL	BIT(_SDEI_MAPF_CRITICAL_SHIFT)

#define SDEI_NORMAL		0
#define SDEI_CRITICAL		1

#define SDEI_NUM_CALLS		32

/* The macros below are used to identify SDEI calls from the SMC function ID */
#define SDEI_FID_MASK		U(0xffe0)
#define SDEI_FID_VALUE		U(0x20)
#define is_sdei_fid(_fid) \
	(((_fid) & SDEI_FID_MASK) == SDEI_FID_VALUE)

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

/*
 * Declare shared entries, and private entries for each core. Also declare a
 * global structure containing private and share entries.
 *
 * This macro must be used in the same file as the platform SDEI mappings are
 * declared. Only then would ARRAY_SIZE() yield a meaningful value.
 * XXX: consider making everything const.
 */
#define DECLARE_SDEI_MAP(_private, _shared) \
	sdei_entry_t sdei_private_event_table \
		[PLATFORM_CORE_COUNT * ARRAY_SIZE(_private)]; \
	sdei_entry_t sdei_shared_event_table[ARRAY_SIZE(_shared)]; \
	const sdei_mapping_t sdei_global_mappings[] = { \
		{ \
			.map = _private, \
			.num_maps = ARRAY_SIZE(_private) \
		}, \
		{ \
			.map = _shared, \
			.num_maps = ARRAY_SIZE(_shared) \
		}, \
	}

/* Runtime data of SDEI event */
typedef struct sdei_entry {
	uint64_t ep;		/* Entry point */
	uint64_t arg;		/* Entry point argument */
	uint64_t affinity;	/* Affinity of shared event */
	unsigned int state;	/* Event handler states: registered, renabled,
				   running */
	unsigned int flags;	/* Registration flags */
	spinlock_t lock;	/* Per event lock */
} sdei_entry_t;

/* Mapping of SDEI events to interrupts, and associated data */
typedef struct sdei_ev_map {
	int32_t ev_num;		/* Event number */
	unsigned int intr;	/* Physical intr number */
	unsigned int flags;	/* Mapping flags, see SDEI_MAPF_* */
	short int usage_cnt;	/* Usage count */
} sdei_ev_map_t;

typedef struct sdei_mapping {
	sdei_ev_map_t *map;
	size_t num_maps;
} sdei_mapping_t;

/* Handler to be called to handle SDEI smc calls */
uint64_t sdei_smc_handler(uint32_t smc_fid,
		uint64_t x1,
		uint64_t x2,
		uint64_t x3,
		uint64_t x4,
		void *cookie,
		void *handle,
		uint64_t flags);

int sdei_init(int crit_desc, int normal_desc);
int sdei_intr_handler(exc_pri_desc_t *desc, uint32_t intr, uint32_t flags,
		void *handle, void *cookie);

#endif /* __SDEI_H__ */
