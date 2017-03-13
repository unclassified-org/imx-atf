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

#include <assert.h>
#include <bl_common.h>
#include <context_mgmt.h>
#include <errno.h>
#include <interrupt_mgmt.h>
#include <platform.h>
#include <platform_ic_extras.h>
#include <stdio.h>
#include <interrupt_class.h>

/* Priority class. Uses only mandatory priority implemented by GIC */
#define PRIORITY_CLASS_SHIFT	4
#define PRIORITY_CLASS_MASK	0x7
#define MAX_INTR_CLASS		8

static interrupt_type_handler_t class_handler[MAX_INTR_CLASS];

/*
 * Priorty-class conversion functions. Interrupts in lower priority classes have
 * higher priority.
 */
unsigned int priority_to_class(unsigned int priority)
{
	return (priority >> PRIORITY_CLASS_SHIFT) & PRIORITY_CLASS_MASK;
}

unsigned int class_to_priority(unsigned int class)
{
	return (class & PRIORITY_CLASS_MASK) << PRIORITY_CLASS_SHIFT;
}

/*******************************************************************************
 * This function triages EL3 interrupts and calls the respective class handler.
 ******************************************************************************/
static uint64_t el3_intr_handler(uint32_t id, uint32_t flags, void *handle,
		void *cookie)
{
	unsigned int priority, priority_class;
	uint64_t status = INTR_ERROR;

	interrupt_type_handler_t handler;

	/* Call back from top-level handler won't read interrupt ID */
	assert(id == INTR_ID_UNAVAILABLE);

	/* Acknowledge the EL3 interrupt */
	id = plat_ic_acknowledge_interrupt();

	/* Find the priority and class */
	priority = plat_ic_get_running_priority();
	priority_class = priority_to_class(priority);

	assert(priority_class < MAX_INTR_CLASS);

	/* Find class handler */
	handler = class_handler[priority_class];

	/* Call the handler */
	if (handler)
		status = handler(id, flags, handle, cookie);

	if (status != INTR_DEFER_EOI) {
		/* End the interrupt */
		plat_ic_end_of_interrupt(id);
	}

	return status;
}

/*******************************************************************************
 * This function must be called to initialize the G0S interrupt partitioning
 * into various classes of priorities.
 ******************************************************************************/
int el3_iclass_init(void)
{
	unsigned int flags = 0;

	/* Select EL3 handling for Secure/Non-Secure interrupts */
	set_interrupt_rm_flag(flags, NON_SECURE);
	set_interrupt_rm_flag(flags, SECURE);

	/* Register handler for EL3 interrupts */
	return register_interrupt_type_handler(INTR_TYPE_EL3, el3_intr_handler,
			flags);
}

/*******************************************************************************
 * This function registers a handler for the 'class' within G0S interrupts. G0S
 * interrupts are typically EL3 handled interrupts, and the various classes are
 * different priority levels among G0S interrupts.
 ******************************************************************************/
int el3_iclass_register_handler(unsigned int class,
		interrupt_type_handler_t handler)
{
	if (class >= MAX_INTR_CLASS)
		return -1;

	class_handler[class] = handler;
	return 0;
}

/*******************************************************************************
 * This function assigns a given interrupt number to a defined class.
 ******************************************************************************/
int el3_iclass_add_intr(unsigned int intr, unsigned int class)
{
	if (class >= MAX_INTR_CLASS)
		return -1;

	if (plat_ic_get_interrupt_type(intr) != INTR_TYPE_EL3)
		return -1;

	plat_ic_set_interrupt_priority(intr, class_to_priority(class));
	return 0;
}

/*******************************************************************************
 * This function removes a given interrupt number from the interrup class.
 ******************************************************************************/
int el3_iclass_remove_intr(unsigned int intr, unsigned int class)
{
	if (plat_ic_get_interrupt_type(intr) != INTR_TYPE_EL3)
		return -1;

	/* TODO: class value is not used here */
	plat_ic_set_interrupt_priority(intr, 0xff);

	return 0;
}

/*******************************************************************************
 * This function masks an interrupt class and sets the flags to be used
 * for unmask and returns if the call actually masked.
 ******************************************************************************/
unsigned int el3_iclass_mask(unsigned int class, unsigned int *flags)
{
	unsigned int mask, new_mask, ret = 0;

	/*
	 * Get the current mask and check if this or higher priority mask is
	 * already in plase
	 */
	mask = plat_ic_get_interrupt_priority_mask();
	new_mask = class_to_priority(class);

	if (new_mask < mask) {
		plat_ic_set_interrupt_priority_mask(new_mask);
		ret = 1;
	}

	*flags = mask;

	return ret;
}

/*******************************************************************************
 * This function unmasks an interrupt class. The 'flags' passed is the value
 * returned from the previous mask.
 ******************************************************************************/
void el3_iclass_unmask(unsigned int flags)
{
	plat_ic_set_interrupt_priority_mask(flags);
}


