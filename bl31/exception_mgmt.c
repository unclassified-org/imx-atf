/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <cpu_data.h>
#include <debug.h>
#include <interrupt_mgmt.h>
#include <ep_info.h>
#include <exception_mgmt.h>
#include <platform.h>

#define EXC_DEBUG	0

#if EXC_DEBUG
# define EXC_LOG(...)	INFO("EXC: " __VA_ARGS__)
#else
# define EXC_LOG(...)
#endif

#define EXC_INVALID_IDX	(-1)

/* sizeof(uint32_t) for now. Can be expanded later to uint64_t if required */
#define STACK_DEPTH		32

/*
 * Exception handlers at EL3, their priority levels, and management.
 */

/* Per-PE exception data */
struct {
	/*
	 * Priority stack, managed as a bitmap.
	 *
	 * Currently only supports 32 priority levels, but the type can be
	 * changed to uint64_t for 64.
	 */
	uint32_t pri_bit_stack;

	/* Priorty mask value before any priority levels were active */
	unsigned int init_pri_mask;
} pe_exc_data[PLATFORM_CORE_COUNT];

/* Translate priority to the index in the priority array */
static int pri_to_idx(unsigned int priority)
{
	int idx;

	idx = EXC_PRI_TO_IDX(priority, exception_data.pri_bits);
	assert(idx < exception_data.num_priorities);

	return idx;
}

/* Translate priority to the index in the priority array */
static int pri_at_idx(int idx)
{
	assert(idx < exception_data.num_priorities);

	return exception_data.exc_priorities[idx].exc_priority;
}

static int pe_priority_idx(unsigned int pe)
{
	uint32_t pri_bit_stack;

	pri_bit_stack = pe_exc_data[pe].pri_bit_stack;
	if (pri_bit_stack == 0)
		return EXC_INVALID_IDX;

	/* Current priority is the right-most bit */
	return __builtin_ctz(pri_bit_stack);
}

/*
 * Return the current active priority.
 */
int exc_current_priority(void)
{
	int idx;

	idx = pe_priority_idx(plat_my_core_pos());
	assert(idx != EXC_INVALID_IDX);

	return pri_at_idx(idx);
}

/*
 * Mark priority active by setting the corresponding bit in pri_bit_stack and
 * programming the priority mask.
 */
void exc_activate_priority(int priority)
{
	int my_pos, idx, cur_pri_idx;
	unsigned int old_mask;

	my_pos = plat_my_core_pos();
	idx = pri_to_idx(priority);
	cur_pri_idx = pe_priority_idx(my_pos);

	/*
	 * Either there should be no active priority, or the request piority
	 * must be of higher (lesser value) than the current one.
	 */
	if ((cur_pri_idx != EXC_INVALID_IDX) && (idx >= cur_pri_idx))
		panic();

	pe_exc_data[my_pos].pri_bit_stack |= BIT(idx);

	/* Program priority mask for the activated level */
	old_mask = plat_ic_set_priority_mask(priority);

	/* Save the priority mask for the first activation */
	if (cur_pri_idx == EXC_INVALID_IDX)
		pe_exc_data[my_pos].init_pri_mask = old_mask;

	EXC_LOG("activate prio=%d\n", pe_priority_idx(my_pos));
}

/*
 * Mark priority inactive by clearing the corresponding bit in pri_bit_stack,
 * and programming the priority mask.
 *
 * Handlers are expected to call this function to deactivate the call.
 */
void exc_deactivate_priority(int priority)
{
	int my_pos, idx, cur_pri_idx;

	/*
	 * Deactivation is only expected to happen when at least one priority
	 * level is active: the current priority.
	 */
	my_pos = plat_my_core_pos();
	idx = pri_to_idx(priority);
	cur_pri_idx = pe_priority_idx(my_pos);

	/*
	 * Deactivation is allowed only when there's an active priority level,
	 * and the requsted priority level must match the current (highest so
	 * far).
	 */
	if ((cur_pri_idx == EXC_INVALID_IDX) || (idx != cur_pri_idx))
		panic();

	/* Clear lowest priority bit */
	pe_exc_data[my_pos].pri_bit_stack &= (pe_exc_data[my_pos].pri_bit_stack - 1);

	/*
	 * Restore priority mask corresponding to the next priority, or the
	 * default one if there are no more to deactivate.
	 */
	idx = pe_priority_idx(my_pos);
	if (idx == EXC_INVALID_IDX)
		plat_ic_set_priority_mask(pe_exc_data[my_pos].init_pri_mask);
	else
		plat_ic_set_priority_mask(pri_at_idx(idx));

	EXC_LOG("deactivate prio=%d\n", pe_priority_idx(my_pos));
}

/*
 * Top-level EL3 interrupt handler.
 */
static uint64_t exc_interrupt_handler(uint32_t id, uint32_t flags, void *handle,
		void *cookie)
{
	int pri, idx, intr, ret = 0;
	exc_pri_desc_t *desc;

	/* Call back from top-level handler won't read interrupt ID */
	assert(id == INTR_ID_UNAVAILABLE);

	/*
	 * Acknowledge interrupt. Proceed with handling only for valid interrupt
	 * IDs. This situation may arise because of Interrupt Management
	 * Framework identifying an EL3 interrupt, but before it's been
	 * acknowledged here, the interrupt was either deasserted, or there was
	 * a higher-priority interrupt of another type.
	 */
	intr = plat_ic_acknowledge_interrupt();
	if (intr >= 1020)
		return 0;

	/* Having acknowledged the interrupt, get the running priority */
	pri = plat_ic_get_running_priority();

	/*
	 * Translate the priority to a descriptor index. We do this by masking
	 * and shifting the running priority value (platform-supplied).
	 */
	idx = pri_to_idx(pri);

	/* Validate priority and levels */
	assert(idx >= 0 && idx < exception_data.num_priorities);
	assert(exception_data.exc_priorities);

	desc = &exception_data.exc_priorities[idx];
	if (desc->exc_handler) {
		/* Activate priority and call handler */
		exc_activate_priority(pri);
		ret = desc->exc_handler(desc, intr, flags, handle, cookie);

		/*
		 * Deactivation might happen only later, when the client of the
		 * dispatcher signals the latter to complete the delegation,
		 * which might only happen in a different path. The dispatcher
		 * is expected to call exc_deactivate_priority() then.
		 */
	}

	return ret;
}

/*
 * Initialize the EL3 exception handling.
 */
int exception_mgmt_init(void)
{
	unsigned int flags = 0;
	int i, ret;
	const exc_pri_desc_t *desc;

	/* Fail initialisation if EL3 interrupts aren't supported */
	if (!plat_ic_has_interrupt_type(INTR_TYPE_EL3))
		return -1;

	/*
	 * Make sure that priority water mark has enough bits to represent the
	 * whole priority array.
	 */
	assert(exception_data.num_priorities <= STACK_DEPTH);

	/*
	 * Bit 7 of GIC priority must be 0 for secure interrupts. This means
	 * platforms must use at least 1 of the remaining 7 bits.
	 */
	if ((exception_data.pri_bits < 1) || (exception_data.pri_bits >= 8)) {
		ERROR("Invalid number of priority bits: %u\n",
				exception_data.pri_bits);
		return -1;
	}

	/*
	 * Iterate through exception priorities and make sure that priorities
	 * are sorted.
	 */
	for (i = 0; i < exception_data.num_priorities; i++) {
		desc = &exception_data.exc_priorities[i];

		/* Validate priority is 8 bits */
		if (desc->exc_priority & ~0xff) {
			ERROR("Invalid priority value 0x%x\n",
					desc->exc_priority);
			return -1;
		}

		/*
		 * Expect the priority value installed at the given index
		 * matches the index. I.e. we should be able to map a priority
		 * to the the index at runtime to locate the handler. This also
		 * validates that the array is sorted by priority.
		 *
		 * Unfilled entries have priority 0, so we ignore them.
		 */
		if ((desc->exc_priority != 0) &&
				pri_to_idx(desc->exc_priority) != i) {
			ERROR("Invalid priority 0x%x at idx 0x%x",
					desc->exc_priority, i);
			return -1;
		}
	}

	/* Route EL3 interrupts when in Secure and Non-secure. */
	set_interrupt_rm_flag(flags, NON_SECURE);
	set_interrupt_rm_flag(flags, SECURE);

	/* Register handler for EL3 interrupts */
	ret = register_interrupt_type_handler(INTR_TYPE_EL3,
			exc_interrupt_handler, flags);

	return ret;
}

/*
 * Register a handler at the supplied priority. Registration is allowed only if
 * a handler hasn't been registered before, or one wasn't provided at build
 * time.
 *
 * Returns 0 on success, or -1 on failure.
 */
int exc_register_priority_handler(int pri, exc_handler_t handler)
{
	int idx;

	idx = pri_to_idx(pri);
	if (idx >= exception_data.num_priorities)
		return -1;

	/* Return failure if a handler was already registered */
	if (exception_data.exc_priorities[idx].exc_handler)
		return -1;

	exception_data.exc_priorities[idx].exc_handler = handler;

	return 0;
}

/* Default exceptions: handle nothing */
DECLARE_EXCEPTIONS(NULL, 0, 0);

/* Bind the default as weak */
#pragma weak exception_data
