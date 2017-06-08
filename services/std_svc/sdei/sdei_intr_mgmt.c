/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <assert.h>
#include <bl_common.h>
#include <context_mgmt.h>
#include <debug.h>
#include <exception_mgmt.h>
#include <interrupt_mgmt.h>
#include <runtime_svc.h>
#include <sdei.h>
#include <string.h>
#include "sdei_private.h"

/* X0-X17 GPREGS context */
#define SDEI_SAVED_GPREGS	18

/* Arbitrary tolerance for shared interrupts received while PE was masked */
#define MAX_MASKED_TRIGGER	(PLATFORM_CORE_COUNT * 32)

/* Maximum preemption nesting levels: Critical priority and Normal priority */
#define MAX_EVENT_NESTING	2

/* Per-CPU SDEI state access macro */
#define sdei_get_this_pe_state()	(&sdei_cpu_state[plat_my_core_pos()])

typedef struct sdei_event_context {
	sdei_ev_map_t *map;
	unsigned int ss;	/* Security state */
	uint64_t x[SDEI_SAVED_GPREGS];

	/* Exception state registers */
	uint64_t elr_el3;
	uint64_t spsr_el3;
} sdei_event_context_t;

/* Per-CPU SDEI state data */
typedef struct sdei_cpu_state {
	sdei_event_context_t event_stack[MAX_EVENT_NESTING];
	unsigned short stack_top;
	unsigned short pe_masked;
	int masked_triggers;
} sdei_cpu_state_t;

/* SDEI states for all cores in the system */
static sdei_cpu_state_t sdei_cpu_state[PLATFORM_CORE_COUNT];

int mask_this_pe(void)
{
	int ret;
	sdei_cpu_state_t *state = sdei_get_this_pe_state();

	ret = state->pe_masked;
	state->pe_masked = 1;

	return ret;
}

int unmask_this_pe(void)
{
	int ret;
	sdei_cpu_state_t *state = sdei_get_this_pe_state();

	ret = state->pe_masked;
	state->pe_masked = 0;
	state->masked_triggers = 0;

	return ret;
}

static unsigned int get_client_el(void)
{
	return read_scr_el3() & SCR_HCE_BIT ? MODE_EL2 : MODE_EL1;
}

static sdei_event_context_t *top_event(int pop)
{
	sdei_cpu_state_t *state = sdei_get_this_pe_state();
	sdei_event_context_t *ev_ctx;

	/* Underflow error */
	if (state->stack_top == 0)
		return NULL;

	ev_ctx = &state->event_stack[state->stack_top - 1];
	if (pop)
		state->stack_top--;

	return ev_ctx;
}

static sdei_event_context_t *pop_event(void)
{
	return top_event(1);
}

static sdei_event_context_t *get_top_event(void)
{
	return top_event(0);
}

static void push_event_ctx(sdei_ev_map_t *map, void *tgt_ctx, int ss)
{
	sdei_event_context_t *ev_ctx;
	gp_regs_t *tgt_gpregs;
	el3_state_t *tgt_el3;
	sdei_cpu_state_t *state = sdei_get_this_pe_state();

	assert(tgt_ctx);
	tgt_gpregs = get_gpregs_ctx(tgt_ctx);
	tgt_el3 = get_el3state_ctx(tgt_ctx);

	/* Cannot have more than max events */
	assert(state->stack_top < MAX_EVENT_NESTING);

	ev_ctx = &state->event_stack[state->stack_top++];
	ev_ctx->ss = ss;
	ev_ctx->map = map;

	/* Save general purpose and exception registers */
	memcpy(ev_ctx->x, tgt_gpregs, sizeof(ev_ctx->x));
	ev_ctx->spsr_el3 = read_ctx_reg(tgt_el3, CTX_SPSR_EL3);
	ev_ctx->elr_el3 = read_ctx_reg(tgt_el3, CTX_ELR_EL3);
}

void restore_event_context(sdei_event_context_t *ev_ctx, void *tgt_ctx)
{
	gp_regs_t *tgt_gpregs;
	el3_state_t *tgt_el3;

	assert(tgt_ctx);
	tgt_gpregs = get_gpregs_ctx(tgt_ctx);
	tgt_el3 = get_el3state_ctx(tgt_ctx);

	memcpy(tgt_gpregs, ev_ctx->x, sizeof(ev_ctx->x));
	write_ctx_reg(tgt_el3, CTX_SPSR_EL3, ev_ctx->spsr_el3);
	write_ctx_reg(tgt_el3, CTX_ELR_EL3, ev_ctx->elr_el3);

#if 0
	SDEI_LOG("%s:%d spsr:%lx elr:%lx\n", __func__, __LINE__,
			ev_ctx->spsr_el3, ev_ctx->elr_el3);
#endif
}

/*
 * This function is used to prepare entry to lower exception level:
 *  - save the current context
 *  - restore the async(sdei/sfiq) target context
 *  - world switch if target context is different from current
 */
static cpu_context_t *world_switch(int tgt_ss)
{
	cpu_context_t *tgt_ctx;

	SDEI_LOG("%s\n", __func__);

	/* Get the target context */
	tgt_ctx = cm_get_context(tgt_ss);
	assert(tgt_ctx);

	/* World switch */
	cm_el1_sysregs_context_save(OTHER_SS(tgt_ss));
	cm_el1_sysregs_context_restore(tgt_ss);
	cm_set_next_eret_context(tgt_ss);

	return tgt_ctx;
}

/*
 * Complete interrupt handling, and deactivate exception priority.
 */
static int complete_interrupt(sdei_ev_map_t *map)
{
	int priority;

	plat_ic_end_of_interrupt(map->intr);

	/* Deactivate the right priority */
	priority = (map->flags & SDEI_MAPF_CRITICAL) ? sdei_critical_pri :
		sdei_normal_pri;
	exc_deactivate_priority(priority);

	return 0;
}

/*
 * SDEI main interrupt handler.
 */
int sdei_intr_handler(exc_pri_desc_t *desc, uint32_t intr, uint32_t flags,
		void *handle, void *cookie)
{
	sdei_entry_t *se;
	cpu_context_t *ctx;
	el3_state_t *el3_ctx;
	sdei_ev_map_t *map;
	sdei_event_context_t *ev_ctx;
	unsigned int ss;
	sdei_cpu_state_t *state = sdei_get_this_pe_state();

	/*
	 * The interrupt has already been acknowledged, and therefore is active,
	 * so no other PE can handle this event while we are at it.
	 */

	/*
	 * Find if this is an SDEI interrupt. There must be an event mapped to
	 * this interrupt
	 */
	map = find_event_map_by_intr(intr, is_spi(intr));
	assert(map);

	se = get_event_entry(map);
	state = sdei_get_this_pe_state();

	if (state->pe_masked) {
		SDEI_LOG("interrupt %u on %lx while PE masked\n", intr,
				read_mpidr_el1());

		/*
		 * For a private event, or for a shared event specifically
		 * routed to this CPU, we leave the interrupt pending, disable
		 * interrupt.
		 */
		if (is_event_private(map) || (se->flags != SDEI_REGF_RM_ANY)) {
			plat_ic_set_interrupt_pending(map->intr);
			/*
			 * This PE is masked. We EOI the interrupt, as it can't be
			 * delegated.
			 */
			complete_interrupt(map);
			return 0;
		}

		/*
		 * We just received a shared event with routing set to ANY PE.
		 * The interrupt can't be delegated here as SDEI events are
		 * masked. However, because its routing mode is ANY, it is
		 * possible that the event can be delegated on any other PE that
		 * hasn't masked events. Therefore, we set the interrupt back
		 * pending so as to give other suitable PEs a chance of handling
		 * it.
		 */
		assert(is_spi(map->intr));
		plat_ic_set_interrupt_pending(map->intr);

		/*
		 * Leaving the same interrupt pending also means that the same
		 * interrupt can target this PE again as soon as this PE leaves
		 * EL3. Whether and how often that happens depends on the
		 * implementation of GIC.
		 *
		 * We therefore keep a count to keep track of how many times
		 * this PE was targeted while events were masked. Should the
		 * count exceed a max value, we disable the interrupt. The
		 * interrupt is later enabled when the PE is unmasked. This is a
		 * more generous approach rather than disabling interrupt
		 * outright.
		 */
		if ((state->masked_triggers < 0) ||
				state->masked_triggers >= MAX_MASKED_TRIGGER) {
			panic();
		} else {
			state->masked_triggers++;
		}

		/*
		 * This PE is masked. We EOI the interrupt, as it can't be
		 * delegated.
		 */
		complete_interrupt(map);

		return 0;
	}

	/*
	 * To handle an event, the following conditions must be true:
	 *
	 * 1. Event must be signalled
	 * 2. Event must be enabled
	 * 3. This PE must be a target PE for the event
	 * 4. PE must be unmasked for SDEI
	 * 5. If this is a normal event, no event must be running
	 * 6. If this is a critical event, no critical event must be running
	 *
	 * (1) and (2) are true when this function is running
	 * (3) is enforced in GIC by selecting the appropriate routing option
	 * (4) is enforced by setting PMR appropriately in GIC
	 * (5) and (6) is enforced using interrupt priority, the RPR, in GIC:
	 *   - Normal SDEI events belong to NSDE priority class
	 *   - Critical SDEI events belong to CSDE priority class
	 */

	/* Some assertions for the above cases */
	assert(!get_ev_state(se, RUNNING));
	ev_ctx = get_top_event();
	if (ev_ctx) {
		assert(is_event_critical(map) && !is_event_critical(ev_ctx->map));
		assert(!is_event_critical(ev_ctx->map));
	}

	ss = get_interrupt_src_ss(flags);
	SDEI_LOG("ACK %lx, ev:%d ss:%d spsr:%lx ELR:%lx\n", read_mpidr_el1(),
			map->ev_num, ss, read_spsr_el3(), read_elr_el3());

	/*
	 * Check to see if disable or unregister happened while we were running,
	 * in which case don't pass the event to client.
	 */
	sdei_event_lock(se, map);
	if (get_ev_state(se, ENABLED) && get_ev_state(se, REGISTERED)) {
		/* Cannot be unregistered now */
		set_ev_state(se, RUNNING);

		/*
		 * FIXME: Handle the race where the interrupt number has changed
		 * between interrupt trigger and getting here.
		 */
	} else {
		/*
		 * States: unregistered, or disabled and not-running. The
		 * interrupt must be already disabled
		 */
		SDEI_LOG("%d: Event is dis/unreg while trying to run: state:%d",
				map->ev_num, get_ev_state_val(se));
		sdei_event_unlock(se, map);

		/*
		 * This assumes that the device level handling of interrupt is
		 * done by the client, otherwise the interrupt will re-trigger
		 * in case of level triggered.
		 */
		return complete_interrupt(map);
	}
	sdei_event_unlock(se, map);

	ctx = handle;

	/*
	 * Check if we interrupted secure state (yielding SMC call). Perform a
	 * context switch so that we can delegate to NS.
	 */
	if (ss == SECURE)
		ctx = world_switch(NON_SECURE);

	/* Push the event and context */
	push_event_ctx(map, ctx, ss);

	/* Set handler arguments */
	SMC_SET_GP(ctx, CTX_GPREG_X0, map->ev_num);
	SMC_SET_GP(ctx, CTX_GPREG_X1, se->arg);

	/* Populate PC and PSTATE arguments from EL3 ELR and SPSR */
	el3_ctx = get_el3state_ctx(ctx);
	SMC_SET_GP(ctx, CTX_GPREG_X2, read_ctx_reg(el3_ctx, CTX_ELR_EL3));
	SMC_SET_GP(ctx, CTX_GPREG_X3, read_ctx_reg(el3_ctx, CTX_SPSR_EL3));

	/* ERET prep */
	cm_set_elr_spsr_el3(NON_SECURE, (uintptr_t) se->ep,
			SPSR_64(get_client_el(), MODE_SP_ELX,
				DISABLE_ALL_EXCEPTIONS));

	/* End of interrupt is done while sdei_event_complete */
	return 0;
}

int sdei_event_complete(int resume, uint64_t arg)
{
	sdei_event_context_t *ev_ctx;
	sdei_entry_t *se;
	sdei_ev_map_t *map;
	cpu_context_t *ctx;

	/* Return error if called without an active event */
	ev_ctx = pop_event();
	if (!ev_ctx)
		return -SDEI_EDENY;

	map = ev_ctx->map;
	assert(map);

	se = get_event_entry(map);

	SDEI_LOG("EOI:%lx, %d spsr:%lx elr:%lx\n", read_mpidr_el1(),
			map->ev_num, read_spsr_el3(), read_elr_el3());

	/* TODO: assert ? the event handler must be running */
	if (!get_ev_state(se, RUNNING))
		return -SDEI_EDENY;

	/* Restore state to non-secure */
	ctx = cm_get_context(NON_SECURE);
	restore_event_context(ev_ctx, ctx);

	/* If it was a COMPLETE_AND_RESUME as exception call */
	if (resume) {
		unsigned int client_el;

		/*
		 * FIXME: complete_and_resume that originally interrupted secure
		 * world is not supported
		 */
		assert(ev_ctx->ss == NON_SECURE);
		client_el = get_client_el();
		cm_set_elr_spsr_el3(NON_SECURE, arg, SPSR_64(client_el,
					MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS));

		/*
		 * Overwrite exception registers of client. The assumption is
		 * that the client, if necessary, would have saved any live
		 * content in these registers before making this call
		 */
		if (client_el == MODE_EL2) {
			write_elr_el2(ev_ctx->elr_el3);
			write_spsr_el2(ev_ctx->spsr_el3);
		} else {
			/* EL1 */
			write_elr_el1(ev_ctx->elr_el3);
			write_spsr_el1(ev_ctx->spsr_el3);
		}
	}

	/* TODO: add callback for subsystems that uses SDEI */

	/*
	 * If delegated the event having interrupted secure world, perform a
	 * world switch back.
	 */
	if (ev_ctx->ss == SECURE)
		ctx = world_switch(SECURE);

	sdei_event_lock(se, map);

	/* Check if event was unregistered while we were running */
	if (!get_ev_state(se, REGISTERED)) {
		sdei_ic_unregister(map);
		unset_sdei_entry(se);
	} /* Else registered and/or enabled and running */

	clr_ev_state(se, RUNNING);
	sdei_event_unlock(se, map);

	return complete_interrupt(map);
}

int sdei_event_context(void *handle, unsigned int param)
{
	sdei_event_context_t *ev_ctx;

	if (param >= SDEI_SAVED_GPREGS)
		return -SDEI_EINVAL;

	/* Get last event in this cpu */
	ev_ctx = get_top_event();
	if (!ev_ctx)
		return -SDEI_EDENY;

	assert(ev_ctx->map);

	/*
	 * No locking is required for the Running status as this is the only CPU
	 * which can complete the event
	 */

	/* Event must have been Running */
	assert(get_ev_state(get_event_entry(ev_ctx->map), RUNNING));

	return ev_ctx->x[param];
}

int sdei_pe_unmask(void)
{
	return unmask_this_pe();
}

int sdei_pe_mask(void)
{
	/* Mask all SDEI (normal and critical) priority events */
	return mask_this_pe();
}
