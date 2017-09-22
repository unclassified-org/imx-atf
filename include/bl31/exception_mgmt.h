/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __EXCEPTION_MGMT_H__
#define __EXCEPTION_MGMT_H__

#include <utils_def.h>
#include <inttypes.h>

#define EXC_PRI_TO_IDX(pri, plat_bits) \
	((pri & 0xff) >> (7 - plat_bits))

#define EXC_DESC_AT(idx)	(&exception_data.exc_priorities[(idx)])
#define EXC_DESC_IDX(desc)	((desc) - exception_data.exc_priorities)

/* Install exception priority at a suitable index */
#define EXC_INSTALL_DESC(plat_bits, priority, handler) \
	[EXC_PRI_TO_IDX(priority, plat_bits)] = { \
	.exc_priority = priority, \
	.exc_handler = handler, \
}

/* Macro for platforms to declare its exception priorities */
#define DECLARE_EXCEPTIONS(priorities, num, bits) \
	const exc_priorities_t exception_data = { \
		.num_priorities = num, \
		.exc_priorities = priorities, \
		.pri_bits = bits, \
	}

/* Forward declaration */
struct exc_pri_desc;

typedef int (*exc_handler_t)(struct exc_pri_desc *desc, uint32_t intr,
		uint32_t flags, void *handle, void *cookie);
typedef int (*exc_to_pri_t)(const int notification_type, void **data_ptr);

typedef struct exc_pri_desc {
	unsigned int exc_priority;
	exc_handler_t exc_handler;
} exc_pri_desc_t;

typedef struct exc_priorities {
	exc_pri_desc_t *exc_priorities;
	unsigned int num_priorities;
	int pri_bits;
} exc_priorities_t;

/* Placeholder declaration to be defined by the platform */
extern const exc_priorities_t exception_data;

int exception_mgmt_init(void);
void exc_activate_priority(int priority);
void exc_deactivate_priority(int priority);
int exc_current_priority(void);
int exc_register_priority_handler(int pri, exc_handler_t handler);

#endif /* __EXCEPTION_MGMT_H__ */
