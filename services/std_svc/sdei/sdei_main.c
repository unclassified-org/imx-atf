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

#include <arch_helpers.h>
#include <assert.h>
#include <atomic.h>
#include <bl_common.h>
#include <bl31.h>
#include <context.h>
#include <context_mgmt.h>
#include <debug.h>
#include <interrupt_class.h>
#include <platform.h>
#include <platform_ic_extras.h>
#include <runtime_svc.h>
#include <sdei.h>
#include "sdei_private.h"
#include <sdei_svc.h>
#include <stddef.h>
#include <string.h>

#define SDEI_DEBUG

#define MAJOR_VERSION	1
#define MINOR_VERSION	0
#define VENDOR_VERSION	0

#define MAKE_SDEI_VERSION(_major, _minor, _vendor) \
	((((unsigned long long)(_major)) << 48) | \
	 (((unsigned long long)(_minor)) << 32) | \
	 (_vendor))

/* TODO: add atomic ops conditional on dynamic events */
#define inc_map_usage(_map)	atomic_add(&((_map)->usage_cnt), 1)
#define dec_map_usage(_map)	atomic_add(&((_map)->usage_cnt), -1)
#define get_map_usage(_map)	((_map)->usage_cnt)

/* FIXME: Add this */
#define is_valid_affinity(_mpidr)	(plat_core_pos_by_mpidr(_mpidr) >= 0)

#define print_map(map)	\
	SDEI_LOG("map:%d intr:%d flags:%d\n", map->ev_num, map->intr, \
			map->flags);

/* Global allocations */
static spinlock_t map_lock;

static void add_iclass(const sdei_ev_map_t *map)
{
	int class;

	/* Dynamic mappings are handled when bind() is called */
	if (is_map_dynamic(map))
		return;

	plat_ic_disable_interrupt(map->intr);
	/* TODO: should we check for active here ? */
	plat_ic_set_interrupt_group(map->intr, INTR_TYPE_EL3);
	class = is_event_critical(map) ? PRIORITY_CLASS_CSDE :
		PRIORITY_CLASS_NSDE;
	el3_iclass_add_intr(map->intr, class);
}

/*******************************************************************************
 * This function is used by platform or subsystem to initialize SDEI
 ******************************************************************************/
int sdei_early_setup(void)
{
	int ret, i;
	const sdei_ev_map_t *map;

	ret = el3_iclass_init();
	if (ret) {
		SDEI_LOG("Error initializing SDEI: iclass failure\n");
		return ret;
	}

	/*
	 * Register interrupt class handler for both Normal and Critical
	 * priority SDE interrupts.
	 */
	el3_iclass_register_handler(PRIORITY_CLASS_NSDE, sdei_intr_handler);
	el3_iclass_register_handler(PRIORITY_CLASS_CSDE, sdei_intr_handler);

	/* TODO: register PM functions */

	init_sdei_state();

	/* Configure static shared platform events */
	for_each_shared_map(i, map) {
		add_iclass(map);
	}

	/* Configure static private platform events for this CPU */
	for_each_private_map(i, map) {
		add_iclass(map);
	}

	SDEI_LOG("Successfully initialized\n");
	return 0;
}

int sdei_cpu_on_init(void)
{
	/* TODO: initialize events */
	/* Mask cpu */
	return 0;
}

void sdei_event_lock(sdei_entry_t *se, sdei_ev_map_t *map)
{
	assert(se != NULL);

	/* No locking required for accessing per-CPU SDEI table */
	if (is_event_private(map))
		return;

	spin_lock(&se->lock);
}

void sdei_event_unlock(sdei_entry_t *se, sdei_ev_map_t *map)
{
	assert(se != NULL);

	/* No locking required for accessing per-CPU SDEI table */
	if (is_event_private(map))
		return;

	spin_unlock(&se->lock);
}

void sdei_map_lock(sdei_ev_map_t *map)
{
	if (!is_map_dynamic(map))
		return;

	spin_lock(&map_lock);
}

void sdei_map_unlock(sdei_ev_map_t *map)
{
	if (!is_map_dynamic(map))
		return;

	spin_unlock(&map_lock);
}

static void set_sdei_entry(sdei_entry_t *se, sdei_ep_t ep, uintptr_t arg,
		int flags, long affinity)
{
	assert(se != NULL);
	se->ep = ep;
	se->arg = arg;
	se->affinity = affinity;
	se->flags = flags;
}

void unset_sdei_entry(sdei_entry_t *se)
{
	assert(se != NULL);
	se->ep = NULL;
	se->arg = 0;
	se->affinity = 0;
	se->flags = 0;
}

void sdei_ic_unregister(sdei_ev_map_t *map)
{
	assert(map);
	assert(is_map_bound(map));

	plat_ic_disable_interrupt(map->intr);
	dec_map_usage(map);

	/*
	 * Any PE routing config is left to the client. As we are disabling the
	 * interrupt, it will be safe to do so
	 */
}

static unsigned long long sdei_version(void)
{
	return MAKE_SDEI_VERSION(MAJOR_VERSION, MINOR_VERSION, VENDOR_VERSION);
}

static int sdei_event_register(int ev_num, sdei_ep_t ep, uint64_t arg,
		int flags, uint64_t mpidr)
{
	sdei_entry_t *se;
	sdei_ev_map_t *map;
	int ret;

	ret = -SMC_EINVAL;
	if (!ep)
		return ret;

	if (flags == SDEI_REGF_RM_PE) {
		if (!is_valid_affinity(mpidr))
			return ret;
	} else if (flags != SDEI_REGF_RM_ANY) {
		/* Unknown flags */
		return ret;
	}

	/* Check if valid event number */
	map = find_event_map(ev_num);
	if (!map)
		return ret;

	/* Early check for dynamic unbound event */
	if (!is_map_bound(map))
		return -SMC_EDENY;

	/* If mapping is valid, lock event map from removing by release() */
	sdei_map_lock(map);
	if (is_map_bound(map)) {
		inc_map_usage(map);
	} else {
		sdei_map_unlock(map);
		return -SMC_EINVAL;
	}
	sdei_map_unlock(map);

	/*
	 * Platform init or bind() must have configured the interrupt in
	 * appropriate group and class.
	 */
	assert(plat_ic_get_interrupt_type(map->intr) == INTR_TYPE_EL3);

	/* FIXME: must already be disabled */
	/* Disable forwarding of new interrupt triggers to cpu interface */
	plat_ic_disable_interrupt(map->intr);

	/*
	 * Early check to see for correct event state. Event must not be
	 * registered or running.
	 */
	se = get_event_entry(map);
	if (get_ev_state(se, REGISTERED) || get_ev_state(se, RUNNING)) {
		dec_map_usage(map);
		return -SMC_EDENY;
	}

	/* Lock event state change */
	sdei_event_lock(se, map);

	/* Recheck to see for desired state */
	if (get_ev_state(se, REGISTERED) || get_ev_state(se, RUNNING))
		goto err_invalid_state;

	/* Meanwhile, did any PE ACK the interrupt? */
	if (plat_ic_get_interrupt_active(map->intr))
		goto err_intr_active;

	/*
	 * Clear any previous interrupt triggers which are pending. This has no
	 * affect on level-triggered interrupts, and does not prevent interrupt
	 * from going pending soon after this.
	 */
	plat_ic_clear_interrupt_pending(map->intr);

	set_sdei_entry(se, ep, arg, flags, mpidr);

	/* Set the routing mode as requested */
	plat_ic_set_interrupt_routing(map->intr, flags == SDEI_REGF_RM_ANY,
			mpidr);

	/* Move to registered and disabled state */
	set_ev_state(se, REGISTERED);
	clr_ev_state(se, ENABLED);
	sdei_event_unlock(se, map);

	ret = 0;
	return ret;

err_intr_active:
	/* We might have forcefully disabled an interrupt here */
err_invalid_state:
	sdei_event_unlock(se, map);
	dec_map_usage(map);
	return ret;
}

static int sdei_event_enable(int ev_num)
{
	sdei_ev_map_t *map;
	sdei_entry_t *se;
	int ret;

	/* Check if valid event number */
	map = find_event_map(ev_num);
	if (!map)
		return -SMC_EINVAL;

	se = get_event_entry(map);

	/* State can be anything except unregistered */
	if (!get_ev_state(se, REGISTERED))
		return -SMC_EDENY;

	/* Already enabled */
	if (get_ev_state(se, ENABLED))
		return 0;

	ret = -SMC_EDENY;
	sdei_event_lock(se, map);
	if (get_ev_state(se, ENABLED)) {
		ret = 0;
	} else if (get_ev_state(se, REGISTERED)) {
		plat_ic_enable_interrupt(map->intr);
		set_ev_state(se, ENABLED);
		ret = 0;
	} /* Else unregistered state */
	sdei_event_unlock(se, map);
	return ret;
}

static int sdei_event_disable(int ev_num)
{
	sdei_ev_map_t *map;
	sdei_entry_t *se;
	int ret = 0;

	/* Check if valid event number */
	map = find_event_map(ev_num);
	if (!map)
		return -SMC_EINVAL;

	se = get_event_entry(map);

	/* State can be anything except unregistered */
	if (!get_ev_state(se, REGISTERED))
		return -SMC_EDENY;

	/* Already disabled */
	if (!get_ev_state(se, ENABLED))
		return 0;

	ret = -SMC_EDENY;
	sdei_event_lock(se, map);
	if (get_ev_state(se, ENABLED)) {
		plat_ic_disable_interrupt(map->intr);
		clr_ev_state(se, ENABLED);
		ret = 0;
	} else if (get_ev_state(se, REGISTERED)) {
		ret = 0;
	} /* Else unregistered state */
	sdei_event_unlock(se, map);
	return ret;
}

static int sdei_event_get_info(int ev_num, int info)
{
	sdei_entry_t *se;
	sdei_ev_map_t *map;

	/* Check if valid event number */
	map = find_event_map(ev_num);
	if (!map)
		return -SMC_EINVAL;

	se = get_event_entry(map);

	switch (info) {
		case SDEI_INFO_EV_TYPE:
			return !is_event_private(map);

		case SDEI_INFO_EV_SIGNALABLE:
			return is_event_signalable(map);

		case SDEI_INFO_EV_CRITICAL:
			return is_event_critical(map);

		case SDEI_INFO_EV_ROUTING_MODE:
			if (is_event_private(map))
				return -SMC_EINVAL;
			if (!get_ev_state(se, REGISTERED))
				return -SMC_EDENY;
			return (se->flags == SDEI_REGF_RM_PE);

		case SDEI_INFO_EV_ROUTING_AFF:
			if (is_event_private(map))
				return -SMC_EINVAL;
			if (!get_ev_state(se, REGISTERED))
				return -SMC_EDENY;
			if (se->flags == SDEI_REGF_RM_PE)
				return -SMC_EINVAL;

			return se->affinity;
		default:
			return -SMC_EINVAL;
	}
}

static int sdei_event_unregister(int ev_num)
{
	int ret = 0;
	sdei_entry_t *se;
	sdei_ev_map_t *map;

	/* Check if valid event number */
	map = find_event_map(ev_num);
	if (!map)
		return -SMC_EINVAL;

	se = get_event_entry(map);

	if (!get_ev_state(se, REGISTERED)) {
		/* Check if already in unregister-pending */
		if (get_ev_state(se, RUNNING)) {
			return -SMC_EPEND;
		} else { /* Unregistered and not-running */
			return -SMC_EDENY;
		}
	}

	sdei_event_lock(se, map);
	if (!get_ev_state(se, REGISTERED)) {
		/* Unregistered */
		if (get_ev_state(se, RUNNING)) {
			/* Unregistered and running, return error pending */
			ret = -SMC_EPEND;
		} else {
			/* Unregistered and not-running */
			ret =  -SMC_EDENY;
		}
	} else {
		/*
		 * States: registered and/or enabled and/or running. Stop
		 * further interrupts
		 */
		plat_ic_disable_interrupt(map->intr);

		/* Clear pending intrs, this could cause spurious intr in ACK */
		plat_ic_clear_interrupt_pending(map->intr);

		/* Move state to unregistered and disabled */
		clr_ev_state(se, ENABLED);
		clr_ev_state(se, REGISTERED);

		if (get_ev_state(se, RUNNING)) {
			/* Leave the complete handler to cleanup */
			ret = -SMC_EPEND;
		} else {
			sdei_ic_unregister(map);
			unset_sdei_entry(se);
		}
	}
	sdei_event_unlock(se, map);

	return ret;
}

static int sdei_event_status(int ev_num)
{
	sdei_ev_map_t *map;
	sdei_entry_t *se;

	/* Check if valid event number */
	map = find_event_map(ev_num);
	if (!map)
		return -SMC_EINVAL;

	se = get_event_entry(map);

	/* TODO: ensure state updates can be seen properly */
	return get_ev_state_val(se);
}

static int sdei_interrupt_bind(int intr_num)
{
	sdei_ev_map_t *map;
	int retry = 1;

	do {
		/*
		 * Bail out if there is already an event for this interrupt,
		 * platform-defined or dynamic
		 */
		map = find_event_map_by_intr(intr_num, is_spi(intr_num));
		if (map) {
			/* Dynamic event, already bound: return event number */
			if (is_map_dynamic(map)) {
				if (is_map_bound(map))
					return map->ev_num;
			} else {
				/* Binding a platform event? */
				return -SMC_EINVAL;
			}
		}

		/* Try to find a free slot */
		map = find_event_map_by_intr(0, is_spi(intr_num));
		if (!map)
			return -SMC_ENOMEM;

		/*
		 * We cannot assert for bound maps here, as we might be racing
		 * with another bind
		 */
		assert(is_map_dynamic(map));

		if (plat_ic_get_interrupt_type(intr_num) != INTR_TYPE_NS)
			return -SMC_EDENY;

		/* Move to EL3 interrupt and disable interrupt */
		plat_ic_set_interrupt_group(intr_num, INTR_TYPE_EL3);
		plat_ic_disable_interrupt(intr_num);

		/* Check to see if interrupt is active */
		if (plat_ic_get_interrupt_active(intr_num))
			goto err_intr_active;

		sdei_map_lock(map);
		if (!is_map_bound(map)) {
			map->intr = intr_num;
			/*
			 * FIXME: look for observer ordering for above and below
			 * statements
			 */
			set_map_bound(map);
			el3_iclass_add_intr(map->intr, PRIORITY_CLASS_NSDE);
			retry = 0;
		}
		sdei_map_unlock(map);
	} while (retry);

	return map->ev_num;

err_intr_active:
	plat_ic_set_interrupt_group(intr_num, INTR_TYPE_NS);
	return -SMC_EDENY;

}

static int sdei_interrupt_release(int ev_num)
{
	int ret;
	sdei_ev_map_t *map;

	/* Check if valid event number */
	map = find_event_map(ev_num);
	print_map(map);
	if (!map)
		return -SMC_EINVAL;

	SDEI_LOG("pre release bound:%d cnt:%d\n", is_map_bound(map),
			(unsigned int) get_map_usage(map));

	if (!is_map_bound(map))
		return -SMC_EINVAL;

	ret = -EINVAL;
	sdei_map_lock(map);
	/* If still unbound */
	if (is_map_bound(map) && get_map_usage(map) == 0) {
		/* Re-assign interrupt for non-secure use */
		el3_iclass_remove_intr(map->intr, PRIORITY_CLASS_NSDE);
		plat_ic_set_interrupt_group(map->intr, INTR_TYPE_NS);
		map->intr = 0;
		clr_map_bound(map);
		ret = 0;
	} else {
		SDEI_LOG("Error release bound:%d cnt:%d\n", is_map_bound(map),
				(unsigned int) get_map_usage(map));
	}
	sdei_map_unlock(map);

	return ret;
}

static int sdei_private_reset(void)
{
	sdei_ev_map_t *map;
	int ret = 0, i;

	/* For each private event, unregister event */
	for_each_private_map(i, map) {
		if (is_map_bound(map)) {
			ret = sdei_event_unregister(map->ev_num);
			/*
			 * The unregister can fail if the event is not
			 * registered, which is allowed. But if the event is
			 * running or unregister-pending then we cannot continue
			 */
			if (ret == -SMC_EPEND)
				return ret;
		}
		map++;
	}

	return 0;
}

static int sdei_shared_reset(void)
{
	const sdei_mapping_t *mapping;
	sdei_ev_map_t *map;
	unsigned int i, j;
	int ret = 0;

	/* For each shared event, unregister and release event */
	for_each_shared_map(i, map) {
		if (is_map_bound(map)) {
			ret = sdei_event_unregister(map->ev_num);
			/*
			 * The unregister can fail if the event is not
			 * registered, which is allowed. But if the event is
			 * running or unregister-pending then we cannot continue
			 */
			if (ret == -SMC_EPEND)
				return ret;
		}
	}

	/* Loop through all mappings and release the dynamic ones */
	for_each_mapping(i, mapping) {
		iterate_mapping(mapping, j, map) {
			if (is_map_dynamic(map)) {
				/* Release the binding */
				ret = sdei_interrupt_release(map->ev_num);

				/*
				 * The error return cannot be deny, which would
				 * mean there is atleast a PE registered for the
				 * event.
				 */
				if (ret == SMC_EDENY)
					return ret;
			}
		}
	}

	return 0;
}

/*******************************************************************************
 * SDEI top level handler for servicing SMCs.
 ******************************************************************************/
uint64_t sdei_smc_handler(uint32_t smc_fid,
			  uint64_t x1,
			  uint64_t x2,
			  uint64_t x3,
			  uint64_t x4,
			  void *cookie,
			  void *handle,
			  uint64_t flags)
{

	uint64_t x5;
	int ss = get_interrupt_src_ss(flags);
	int64_t ret;
	unsigned int resume = 0;

	/*
	 * FIXME: Validate interface caller exception level
	 * 	For  ss=1, if EL2 is present, then EL2 otherwise EL1
	 *	     ss=0, SEL1
	 */
	if (ss != NON_SECURE)
		SMC_RET1(handle, SMC_UNK);

	/* Only 64 bit sdei calls are supported */
	switch (smc_fid) {
	case SDEI_VERSION:
		ret = sdei_version();
		SDEI_LOG("VER:%lx\n", ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_EVENT_REGISTER:
		x5 = SMC_GET_GP(handle, CTX_GPREG_X5);
		ret = sdei_event_register(x1, (sdei_ep_t) x2, x3, x4, x5);
		SDEI_LOG("REG(n:%d e:%lx a:%lx f:%x m:%lx) = %ld\n", (int) x1,
				x2, x3, (int) x4, x5, ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_EVENT_ENABLE:
		ret = sdei_event_enable(x1);
		SDEI_LOG("ENA(n:%d)=%ld\n", (int) x1, ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_EVENT_DISABLE:
		ret = sdei_event_disable(x1);
		SDEI_LOG("DIS(n:%d)=%ld\n", (int) x1, ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_EVENT_CONTEXT:
		ret = sdei_event_context(handle, x1);
		SDEI_LOG("CTX(p:%d):%lx=%ld\n", (int) x1, read_mpidr_el1(), ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_EVENT_COMPLETE_AND_RESUME:
		resume = 1;
		/* Fall through */

	case SDEI_EVENT_COMPLETE:
		ret = sdei_event_complete(resume, x1);
		SDEI_LOG("CLT(r:%d sta/ep:%lx):%lx=%ld\n", resume, x1,
				read_mpidr_el1(), ret);
		/* Set the return only in case of error */
		if (ret)
			SMC_RET1(handle, ret);

		SMC_RET0(handle);
		break;

	case SDEI_EVENT_STATUS:
		ret = sdei_event_status(x1);
		SDEI_LOG("STA(n:%d)=%ld\n", (int) x1, ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_EVENT_GET_INFO:
		ret = sdei_event_get_info(x1, x2);
		SDEI_LOG("INF(n:%d, %d)=%ld\n", (int) x1, (int) x2, ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_EVENT_UNREGISTER:
		ret = sdei_event_unregister(x1);
		SDEI_LOG("UNR(n:%d)=%ld\n", (int) x1, ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_PE_UNMASK:
		ret = sdei_pe_unmask();
		SDEI_LOG("UNMASK:%lx = %ld\n", read_mpidr_el1(), ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_PE_MASK:
		ret = sdei_pe_mask();
		SDEI_LOG("MASK:%lx = %ld\n", read_mpidr_el1(), ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_INTERRUPT_BIND:
		ret = sdei_interrupt_bind(x1);
		SDEI_LOG("BIND(%d) = %ld\n", (int) x1, ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_INTERRUPT_RELEASE:
		ret = sdei_interrupt_release(x1);
		SDEI_LOG("REL(%d) = %ld\n", (int) x1, ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_SHARED_RESET:
		ret = sdei_shared_reset();
		SDEI_LOG("S_RESET():%lx = %ld\n", read_mpidr_el1(), ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_PRIVATE_RESET:
		ret = sdei_private_reset();
		SDEI_LOG("P_RESET():%lx = %ld\n", read_mpidr_el1(), ret);
		SMC_RET1(handle, ret);
		break;

#if 0
	case SDEI_EVENT_ROUTING_SET:
		ret = sdei_event_routeing_set(x1, x2, x3);
		SDEI_LOG("sdei_ev_ri_s(n:%d f:%lx aff:%lx)=%d",
						(int)x1, x2, x3, ret);
		SMC_RET1(handle, ret);
		break;
#endif
#ifdef SDEI_DEBUG
	case SDEI_SHOW_DEBUG:
		SDEI_LOG("DBG(%lx, %lx, %lx)\n", x1, x2, x3);
		SMC_RET0(handle);
		break;
#endif

	case SDEI_EVENT_SIGNAL:
	case SDEI_FEATURES:
	default:
		break;
	}

	WARN("Unimplemented SDEI Call: 0x%x \n", smc_fid);
	SMC_RET1(handle, SMC_UNK);
}

