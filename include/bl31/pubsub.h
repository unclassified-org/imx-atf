/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __PUBSUB_H__
#define __PUBSUB_H__

#define __pubsub_start_sym(event)	__pubsub_##event##_start
#define __pubsub_end_sym(event)		__pubsub_##event##_end

#ifdef __LINKER__

/* For the linker ... */

#define __pubsub_section(event)		__pubsub_##event

/* Collect pubsub sections, and place guard symbols around */
#define DEFINE_PUBSUB_EVENT(event) \
	__pubsub_start_sym(event) = .; \
	KEEP(*(__pubsub_section(event))); \
	__pubsub_end_sym(event) = .

#else /* __LINKER__ */

/* For the compiler ... */

#include <arch_helpers.h>
#include <assert.h>
#include <cdefs.h>
#include <stddef.h>

#define __pubsub_section(event)		__section("__pubsub_" #event)

/*
 * Have the function func called back when the specified event happens. This
 * macro places the function address into the pubsub section, which is picked up
 * and invoked by the invoke_pubsubs() function via. the PUBLISH_EVENT* macros.
 */
#define SUBSCRIBE_TO_EVENT(event, func) \
	pubsub_cb_t __cb_func_##func##event __pubsub_section(event) = func

/*
 * Iterate over subscribed handlers for a defined event. 'event' is the name of
 * the event, and 'subscriber' a local variable of type 'pubsub_cb_t *'.
 */
#define for_each_subscriber(event, subscriber) \
	for (subscriber = __pubsub_start_sym(event); \
			subscriber < __pubsub_end_sym(event); \
			subscriber++)

/*
 * Publish a defined event with supplied argument, with the intent of
 * resolution. All subscribed handlers are invoked until one returns non-zero
 * value. The return value of the macro is the return value of the last invoked
 * handler.
 */
#define PUBLISH_EVENT_TO_RESOLVE_ARG(event, arg) \
	__extension__ ({ \
		pubsub_cb_t *subscriber; \
		void *cb_ret; \
		for_each_subscriber(event, subscriber) { \
			cb_ret = (*subscriber)(arg); \
			if (cb_ret) \
				break; \
		} \
		cb_ret; \
	})

/* Publish a defined event to resolve, with NULL argument */
#define PUBLISH_EVENT_TO_RESOLVE(event) \
	PUBLISH_EVENT_TO_RESOLVE_ARG(event, NULL)

/*
 * Publish a defined event supplying an argument. All subscribed handlers are
 * invoked, but the macro returns NULL;
 */
#define PUBLISH_EVENT_ARG(event, arg) \
	__extension__ ({ \
		pubsub_cb_t *subscriber; \
		for_each_subscriber(event, subscriber) { \
			(*subscriber)(arg); \
		} \
		NULL; \
	})

/* Publish a defined event with NULL argument */
#define PUBLISH_EVENT(event)	PUBLISH_EVENT_ARG(event, NULL)

/* Define an event to enable publishing and subscribing to */
#define DEFINE_PUBSUB_EVENT(event) \
	extern pubsub_cb_t __pubsub_start_sym(event)[]; \
	extern pubsub_cb_t __pubsub_end_sym(event)[];

/* Subscriber callback type */
typedef void* (*pubsub_cb_t)(const void *arg);

#include <pubsub_events.h>

#endif	/* __LINKER__ */
#endif	/* __PUBSUB_H__ */
