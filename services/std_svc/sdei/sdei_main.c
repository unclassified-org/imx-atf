/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <assert.h>
#include <bl_common.h>
#include <bl31.h>
#include <context.h>
#include <context_mgmt.h>
#include <debug.h>
#include <interrupt_mgmt.h>
#include <platform.h>
#include <pubsub.h>
#include <runtime_svc.h>
#include <sdei.h>
#include <sdei.h>
#include <stddef.h>
#include <string.h>
#include "sdei_private.h"

#ifdef AARCH32
# error SDEI is defined only for AAarch64 systems
#endif

/* Function that validates SDEI entrypoints on the platform */
#pragma weak plat_validate_sdei_entry_point

#define SDEI_EVENT_0	0

#define MAJOR_VERSION	1
#define MINOR_VERSION	0
#define VENDOR_VERSION	0

#define MAKE_SDEI_VERSION(_major, _minor, _vendor) \
	((((unsigned long long)(_major)) << 48) | \
	 (((unsigned long long)(_minor)) << 32) | \
	 (_vendor))

#define LOWEST_INTR_PRIORITY		0xff

#define is_valid_affinity(_mpidr)	(plat_core_pos_by_mpidr(_mpidr) >= 0)

#define print_map(map)	\
	SDEI_LOG("map:%d intr:%d flags:%d\n", map->ev_num, map->intr, \
			map->flags);

unsigned int sdei_normal_pri, sdei_critical_pri;
static unsigned int num_priv_slots, num_shrd_slots;

/* Only one lock to serialize all mapping accesses */
static spinlock_t map_lock;

unsigned int class_to_priority(unsigned int class)
{
	return class == SDEI_NORMAL ? sdei_normal_pri : sdei_critical_pri;
}

static void set_intr_priority(unsigned int intr, unsigned int class)
{
	assert(plat_ic_get_interrupt_type(intr) == INTR_TYPE_EL3);

	plat_ic_set_interrupt_priority(intr, class_to_priority(class));
}

static void remove_interrupt(unsigned int intr, unsigned int class)
{
	assert(plat_ic_get_interrupt_type(intr) == INTR_TYPE_EL3);

	plat_ic_set_interrupt_priority(intr, LOWEST_INTR_PRIORITY);
}

static void init_map(sdei_ev_map_t *map)
{
	map->usage_cnt = 0;
}

int event_to_priority(sdei_ev_map_t *map)
{
	return is_event_critical(map) ? SDEI_CRITICAL : SDEI_NORMAL;
}

static int sdei_pri_init(int sdei_pri)
{
	int i, ev_num_so_far, zero_found = 0;
	sdei_ev_map_t *map;

	/* Sanity check and configuration of shared events */
	ev_num_so_far = -1;
	for_each_shared_map(i, map) {
		if ((ev_num_so_far >= 0) && (map->ev_num <= ev_num_so_far)) {
			ERROR("Shared mapping not sorted\n");
			return -1;
		}
		ev_num_so_far = map->ev_num;

		if (map->ev_num == SDEI_EVENT_0) {
			ERROR("Event 0 in shared mapping\n");
			return -1;
		}

		if (map->ev_num < 0) {
			ERROR("Invalid shared event at %d: %d\n", i,
					map->ev_num);
			return -1;
		}

		if (is_event_private(map)) {
			ERROR("Private event %d in shared table\n",
					map->ev_num);
			return -1;
		}

		/* Skip initializing the wrong priority */
		if (event_to_priority(map) != sdei_pri)
			continue;

		/* Platform events are always bound, so set the bound flag */
		if (!is_map_dynamic(map)) {
			/* Shared mappings must be bound to shared interrupt */
			if (!is_spi(map->intr)) {
				ERROR("Invalid shared binding for IRQ %u\n",
						map->intr);
				return -1;
			}

			set_map_bound(map);
		} else {
			num_shrd_slots++;
		}

		init_map(map);
	}

	/* Sanity check and configuration of private events for this CPU */
	ev_num_so_far = -1;
	for_each_private_map(i, map) {
		if ((ev_num_so_far >= 0) && (map->ev_num <= ev_num_so_far)) {
			ERROR("Shared mapping not sorted\n");
			return -1;
		}
		ev_num_so_far = map->ev_num;

		if (map->ev_num == SDEI_EVENT_0) {
			zero_found = 1;
			if (!is_secure_sgi(map->intr)) {
				ERROR("Event 0 must bind to secure SGI\n");
				return -1;
			}

			/*
			 * XXX: check for flag to be exactly
			 * SDEI_MAPF_SIGNALABLE?
			 */
		}

		if (map->ev_num < 0) {
			ERROR("Invalid private event at %d: %d\n", i,
					map->ev_num);
			return -1;
		}

		if (!is_event_private(map)) {
			ERROR("Shared event %d in shared table\n",
					map->ev_num);
			return -1;
		}

		/* Skip initializing the wrong priority */
		if (event_to_priority(map) != sdei_pri)
			continue;

		/* Platform events are always bound, so set the bound flag */
		if(!is_map_dynamic(map)) {
			/* Private mappings must be bound to private interrupt */
			if (is_spi(map->intr)) {
				ERROR("Invalid private binding for IRQ %u\n",
						map->intr);
				return -1;
			}

			set_map_bound(map);
		} else {
			num_priv_slots++;
		}

		init_map(map);
	}

	if (!zero_found) {
		ERROR("Event 0 not found in private mapping\n");
		return -1;
	}

	/* Mask this PE on upon cold boot */
	mask_this_pe();

	return 0;
}

int sdei_init(int critical_pri, int normal_pri)
{
	int ret;

	assert(critical_pri < normal_pri);
	sdei_critical_pri = critical_pri;
	sdei_normal_pri = normal_pri;

	ret = sdei_pri_init(SDEI_CRITICAL);
	if (ret)
		return ret;

	ret = sdei_pri_init(SDEI_NORMAL);
	if (ret)
		return ret;

	return 0;
}

static void *sdei_cpu_on_init(const void *arg)
{
	int i;
	sdei_ev_map_t *map;
	sdei_entry_t *se;

	/* Initialize private mappings on this CPU */
	for_each_private_map(i, map) {
		se = get_event_entry(map);
		unset_sdei_entry(se);
	}

	SDEI_LOG("Private events initialized on %lx\n", read_mpidr_el1());

	/* All PEs start with SDEI events masked */
	mask_this_pe();

	return 0;
}

void sdei_event_lock(sdei_entry_t *se, sdei_ev_map_t *map)
{
	assert(se);
	assert(map);

	/* No locking required for accessing per-CPU SDEI table */
	if (is_event_private(map))
		return;

	spin_lock(&se->lock);
}

void sdei_event_unlock(sdei_entry_t *se, sdei_ev_map_t *map)
{
	assert(se);
	assert(map);

	/* No locking required for accessing per-CPU SDEI table */
	if (is_event_private(map))
		return;

	spin_unlock(&se->lock);
}

void sdei_map_lock(sdei_ev_map_t *map)
{
	/*
	 * No need to grab lock if mapping is a static. No run-time bindings
	 * permitted on this.
	 */
	if (map && !is_map_dynamic(map))
		return;

	spin_lock(&map_lock);
}

void sdei_map_unlock(sdei_ev_map_t *map)
{
	/*
	 * No need to grab lock if mapping is a static. No run-time bindings
	 * permitted on this.
	 */
	if (map && !is_map_dynamic(map))
		return;

	spin_unlock(&map_lock);
}

static void set_sdei_entry(sdei_entry_t *se, uint64_t ep, uintptr_t arg,
		unsigned int flags, uint64_t affinity)
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
	se->ep = 0;
	se->arg = 0;
	se->affinity = 0;
	se->flags = 0;
}

/* Temporarily pin a map to prevent its usage count dropping to 0 */
static int pin_map(sdei_ev_map_t *map)
{
	sdei_map_lock(map);
	if (is_map_bound(map)) {
		/*
		 * If mapping is valid, prevent lock event map from removing by
		 * release() by temporarily incrementing reference count.
		 */
		map->usage_cnt++;
	} else {
		/* Disallow registering for an unbound map */
		sdei_map_unlock(map);
		return -SDEI_EDENY;
	}
	sdei_map_unlock(map);

	return 0;
}

static void unpin_map(sdei_ev_map_t *map)
{
	/* Undo the temporary ref count increment above */
	sdei_map_lock(map);
	map->usage_cnt--;
	assert(map->usage_cnt >= 0);
	sdei_map_unlock(map);
}

void sdei_ic_unregister(sdei_ev_map_t *map)
{
	assert(map);
	assert(is_map_bound(map));

	plat_ic_disable_interrupt(map->intr);
	unpin_map(map);

	/*
	 * Any PE routing config is left to the client. As we are disabling the
	 * interrupt, it will be safe to do so.
	 */
}

static unsigned long long sdei_version(void)
{
	return MAKE_SDEI_VERSION(MAJOR_VERSION, MINOR_VERSION, VENDOR_VERSION);
}

/*
 * Default Function to validate SDEI entry point. Platforms may override this.
 */
int plat_validate_sdei_entry_point(uintptr_t ep)
{
	return 0;
}

/* Validate flags and MPIDR values for REGISTER and SET_ROUTING calls */
static int validate_flags(uint64_t flags, uint64_t mpidr)
{
	/* Validate flags */
	switch (flags) {
	case SDEI_REGF_RM_PE:
		if (!is_valid_affinity(mpidr))
			return -SDEI_EINVAL;
		break;
	case SDEI_REGF_RM_ANY:
		break;
	default:
		/* Unknown flags */
		return -SDEI_EINVAL;
	}

	return 0;
}

static int sdei_event_routing_set(int ev_num, uint64_t flags, uint64_t mpidr)
{
	int ret, routing;
	sdei_ev_map_t *map;
	sdei_entry_t *se;

	ret = validate_flags(flags, mpidr);
	if (ret)
		return ret;

	/* Check if valid event number */
	map = find_event_map(ev_num);
	if (!map)
		return -SDEI_EINVAL;

	/* The event must be private */
	if (is_event_private(map))
		return -SDEI_EINVAL;

	ret = pin_map(map);
	if (ret)
		return ret;

	se = get_event_entry(map);

	sdei_event_lock(se, map);
	if (!get_ev_state(se, REGISTERED))
		goto not_registered;

	/* Choose appropriate routing */
	routing = (flags == SDEI_REGF_RM_ANY) ? INTR_ROUTING_MODE_ANY :
		INTR_ROUTING_MODE_PE;

	/* Reprogram routing after disabling interrupt, and re-enable it */
	plat_ic_disable_interrupt(map->intr);
	plat_ic_set_spi_routing(map->intr, routing, mpidr);
	plat_ic_enable_interrupt(map->intr);

	sdei_event_unlock(se, map);
	unpin_map(map);

	return 0;

not_registered:
	sdei_event_unlock(se, map);
	unpin_map(map);

	return -SDEI_EDENY;
}

static int sdei_event_register(int ev_num, uint64_t ep, uint64_t arg,
		uint64_t flags, uint64_t mpidr)
{
	int ret;
	sdei_entry_t *se;
	sdei_ev_map_t *map;

	if (!ep || plat_validate_sdei_entry_point(ep))
		return -SDEI_EINVAL;

	ret = validate_flags(flags, mpidr);
	if (ret)
		return ret;

	/* Check if valid event number */
	map = find_event_map(ev_num);
	if (!map)
		return -SDEI_EINVAL;

	/* Private events always target the PE */
	if (is_event_private(map))
		flags = SDEI_REGF_RM_PE;

	ret = pin_map(map);
	if (ret)
		return ret;

	/* Disable forwarding of new interrupt triggers to cpu interface */
	plat_ic_disable_interrupt(map->intr);

	se = get_event_entry(map);

	/* Lock event state change */
	sdei_event_lock(se, map);

	/* Check for desired state */
	if (get_ev_state(se, REGISTERED) || get_ev_state(se, RUNNING))
		goto err_invalid_state;

	/* Move to EL3 interrupt and disable interrupt */
	plat_ic_disable_interrupt(map->intr);

	/* Meanwhile, did any PE ACK the interrupt? */
	if (plat_ic_get_interrupt_active(map->intr))
		goto err_intr_active;

	/*
	 * Any events that are triggered after register and before enable should
	 * remain pending. Clear any previous interrupt triggers which are
	 * pending. This has no affect on level-triggered interrupts to start
	 * with a clean slate.
	 */
	if (ev_num != SDEI_EVENT_0)
		plat_ic_clear_interrupt_pending(map->intr);

	/* Map interrupt to EL3 and program the correct priority */
	plat_ic_set_interrupt_type(map->intr, INTR_TYPE_EL3);
	set_intr_priority(map->intr, SDEI_NORMAL);

	/* Populate event entries */
	set_sdei_entry(se, ep, arg, flags, mpidr);

	/*
	 * Set the routing mode for shared event, as requested. We already
	 * ensure that shared events get bound to SPIs.
	 */
	if (!is_event_private(map)) {
		plat_ic_set_spi_routing(map->intr,
				flags == SDEI_REGF_RM_ANY, mpidr);
	}

	/* Move to registered and disabled state */
	set_ev_state(se, REGISTERED);
	clr_ev_state(se, ENABLED);
	sdei_event_unlock(se, map);

	/* Mapping unpinned when event is unregistered */

	return 0;

err_intr_active:
	/* We might have forcefully disabled an interrupt here */
err_invalid_state:
	sdei_event_unlock(se, map);
	unpin_map(map);

	return -SDEI_EDENY;
}

static int sdei_event_enable(int ev_num)
{
	sdei_ev_map_t *map;
	sdei_entry_t *se;
	int ret;

	/* Check if valid event number */
	map = find_event_map(ev_num);
	if (!map)
		return -SDEI_EINVAL;

	se = get_event_entry(map);

	ret = -SDEI_EDENY;
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
		return -SDEI_EINVAL;

	se = get_event_entry(map);

	ret = -SDEI_EDENY;
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

	unsigned int flags, registered;
	unsigned long long affinity;

	/* Check if valid event number */
	map = find_event_map(ev_num);
	if (!map)
		return -SDEI_EINVAL;

	/* The event must be bound */
	if(!is_map_bound(map))
		return -SDEI_EINVAL;

	se = get_event_entry(map);

	/* Sample state under lock */
	sdei_event_lock(se, map);
	registered = get_ev_state(se, REGISTERED);
	flags = se->flags;
	affinity = se->affinity;
	sdei_event_unlock(se, map);

	switch (info) {
	case SDEI_INFO_EV_TYPE:
		return !is_event_private(map);

	case SDEI_INFO_EV_SIGNALED:
		return is_event_signalable(map);

	case SDEI_INFO_EV_PRIORITY:
		return is_event_critical(map);

	case SDEI_INFO_EV_ROUTING_MODE:
		if (is_event_private(map))
			return -SDEI_EINVAL;
		if (!registered)
			return -SDEI_EDENY;
		return (flags == SDEI_REGF_RM_PE);

	case SDEI_INFO_EV_ROUTING_AFF:
		if (is_event_private(map))
			return -SDEI_EINVAL;
		if (!registered)
			return -SDEI_EDENY;
		if (flags == SDEI_REGF_RM_PE)
			return -SDEI_EINVAL;
		return affinity;

	default:
		return -SDEI_EINVAL;
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
		return -SDEI_EINVAL;

	se = get_event_entry(map);

	sdei_event_lock(se, map);
	if (!get_ev_state(se, REGISTERED)) {
		/* Unregistered */
		if (get_ev_state(se, RUNNING)) {
			/* Unregistered and running, return error pending */
			ret = -SDEI_EPEND;
		} else {
			/* Unregistered and not-running */
			ret =  -SDEI_EDENY;
		}
	} else {
		/*
		 * States: registered and/or enabled and/or running. Stop
		 * further interrupts.
		 */
		plat_ic_disable_interrupt(map->intr);

		/* Clear pending intrs. This could cause spurious intr in ACK */
		if (ev_num != SDEI_EVENT_0)
			plat_ic_clear_interrupt_pending(map->intr);

		/* Move state to unregistered and disabled */
		clr_ev_state(se, ENABLED);
		clr_ev_state(se, REGISTERED);

		if (get_ev_state(se, RUNNING)) {
			/* Leave the complete handler to cleanup */
			ret = -SDEI_EPEND;
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
		return -SDEI_EINVAL;

	se = get_event_entry(map);

	return get_ev_state_val(se);
}

static int sdei_interrupt_bind(int intr_num)
{
	sdei_ev_map_t *map;
	int retry = 1;

	/* SGIs are not allowed to be bound */
	if (is_sgi(intr_num))
		return -SDEI_EINVAL;

	do {
		/*
		 * Bail out if there is already an event for this interrupt,
		 * either platform-defined or dynamic.
		 */
		map = find_event_map_by_intr(intr_num, is_spi(intr_num));
		if (map) {
			if (is_map_dynamic(map)) {
				if (is_map_bound(map)) {
					/*
					 * Dynamic event, already bound. Return
					 * event number.
					 */
					return map->ev_num;
				}
			} else {
				/* Binding non-dynamic event */
				return -SDEI_EINVAL;
			}
		}

		/* Try to find a free slot */
		map = find_event_map_by_intr(0, is_spi(intr_num));
		if (!map)
			return -SDEI_ENOMEM;

		/* The returned mapping must be dynamic */
		assert(is_map_dynamic(map));

		/*
		 * We cannot assert for bound maps here, as we might be racing
		 * with another bind.
		 */

		/* The requested interrupt must already belong to NS */
		if (plat_ic_get_interrupt_type(intr_num) != INTR_TYPE_NS)
			return -SDEI_EDENY;

		sdei_map_lock(map);
		if (!is_map_bound(map)) {
			map->intr = intr_num;
			set_map_bound(map);
			retry = 0;
		}
		sdei_map_unlock(map);
	} while (retry);

	return map->ev_num;
}

static int sdei_interrupt_release(int ev_num)
{
	int ret;
	sdei_ev_map_t *map;

	/* Check if valid event number */
	map = find_event_map(ev_num);
	if (!map)
		return -SDEI_EINVAL;

	if (!is_map_dynamic(map))
		return -SDEI_EINVAL;

	print_map(map);

	sdei_map_lock(map);
	SDEI_LOG("Trying to release bound: usage cnt:%d\n", map->usage_cnt);

	/* If still unbound */
	if (is_map_bound(map) && (map->usage_cnt == 0)) {
		/* Re-assign interrupt for non-secure use */
		remove_interrupt(map->intr, SDEI_NORMAL);
		plat_ic_set_interrupt_type(map->intr, INTR_TYPE_NS);
		map->intr = 0;
		clr_map_bound(map);
		ret = 0;
	} else {
		SDEI_LOG("Error release bound:%d cnt:%d\n", is_map_bound(map),
				map->usage_cnt);
		ret = -SDEI_EINVAL;
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
			 * running or unregister-pending, we cannot continue.
			 * All other errors are ignored.
			 */
			if (ret == -SDEI_EPEND)
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
			/* Release the binding */
			ret = sdei_event_unregister(map->ev_num);

			/*
			 * The unregister can fail if the event is not
			 * registered, which is allowed. But if the event is
			 * running or unregister-pending, we cannot continue.
			 * All other errors are ignored.
			 */
			if (ret == -SDEI_EPEND)
				return ret;
		}
	}

	/* Loop through all mappings and release the dynamic events */
	for_each_mapping_type(i, mapping) {
		iterate_mapping(mapping, j, map) {
			if (is_map_dynamic(map)) {
				/* Release the binding */
				ret = sdei_interrupt_release(map->ev_num);

				/*
				 * The error return cannot be deny, which would
				 * mean there is at least a PE registered for
				 * the event.
				 */
				if (ret == SDEI_EDENY)
					return ret;
			}
		}
	}

	return 0;
}

int sdei_signal(int event, uint64_t target_pe)
{
	sdei_ev_map_t *map;

	/* Only event 0 can be signalled */
	if (event != SDEI_EVENT_0)
		return -SDEI_EINVAL;

	/* Find mapping for event 0 */
	map = find_event_map(SDEI_EVENT_0);
	if (!map)
		return -SDEI_EINVAL;

	/* The event must be signalable */
	if (!is_event_signalable(map))
		return -SDEI_EINVAL;

	/* Validate target */
	if (plat_core_pos_by_mpidr(target_pe) < 0)
		return -SDEI_EINVAL;

	/* Raise SGI. Platform will validate target_pe */
	plat_ic_raise_el3_sgi(map->intr, target_pe);

	return 0;
}

uint64_t sdei_features(unsigned int feature)
{
	if (feature == SDEI_FEATURE_BIND_SLOTS)
		return FEATURE_BIND_SLOTS(num_priv_slots, num_shrd_slots);

	return SDEI_EINVAL;
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
		ret = sdei_event_register(x1, x2, x3, x4, x5);
		SDEI_LOG("REG(n:%d e:%lx a:%lx f:%x m:%lx) = %ld\n", (int) x1,
				x2, x3, (int) x4, x5, ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_EVENT_ENABLE:
		ret = sdei_event_enable(x1);
		SDEI_LOG("ENABLE(n:%d)=%ld\n", (int) x1, ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_EVENT_DISABLE:
		ret = sdei_event_disable(x1);
		SDEI_LOG("DISABLE(n:%d)=%ld\n", (int) x1, ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_EVENT_CONTEXT:
		ret = sdei_event_context(handle, x1);
		SDEI_LOG("CTX(p:%d):%lx=%ld\n", (int) x1, read_mpidr_el1(),
				ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_EVENT_COMPLETE_AND_RESUME:
		resume = 1;
		/* Fall through */

	case SDEI_EVENT_COMPLETE:
		ret = sdei_event_complete(resume, x1);
		SDEI_LOG("COMPLETE(r:%d sta/ep:%lx):%lx=%ld\n", resume, x1,
				read_mpidr_el1(), ret);
		/* Set the return only in case of error */
		if (ret)
			SMC_RET1(handle, ret);

		SMC_RET0(handle);
		break;

	case SDEI_EVENT_STATUS:
		ret = sdei_event_status(x1);
		SDEI_LOG("STAT(n:%d)=%ld\n", (int) x1, ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_EVENT_GET_INFO:
		ret = sdei_event_get_info(x1, x2);
		SDEI_LOG("INFO(n:%d, %d)=%ld\n", (int) x1, (int) x2, ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_EVENT_UNREGISTER:
		ret = sdei_event_unregister(x1);
		SDEI_LOG("UNREG(n:%d)=%ld\n", (int) x1, ret);
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

	case SDEI_EVENT_ROUTING_SET:
		ret = sdei_event_routing_set(x1, x2, x3);
		SDEI_LOG("ROUTE_SET(n:%d f:%lx aff:%lx) = %ld\n", (int) x1, x2,
				x3, ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_FEATURES:
		ret = sdei_features(x1);
		SDEI_LOG("FTRS = %lx\n", ret);
		SMC_RET1(handle, ret);
		break;

	case SDEI_EVENT_SIGNAL:
		ret = sdei_signal(x1, x2);
		SDEI_LOG("SIGNAL e:%lx t:%lx ret:%lx\n", x1, x2, ret);
		SMC_RET1(handle, ret);
		break;
	default:
		break;
	}

	WARN("Unimplemented SDEI Call: 0x%x \n", smc_fid);
	SMC_RET1(handle, SMC_UNK);
}

/* Subscribe to PSCI CPU on to initialize per-CPU SDEI configuration */
SUBSCRIBE_TO_EVENT(psci_cpu_on_finish, sdei_cpu_on_init);
