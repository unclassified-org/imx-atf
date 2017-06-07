
SDEI Prototype Implementation in ARM Trusted Firmware
=====================================================

# Introduction
Software Delegated Exception Interface (SDEI) is an interface which allows
hypervisor and OS (referred as Client) to register and manage high priority
events. The events are of higher priority than the clients own interrupts
and provides a mechanism for clients to execute code even in their critical
section with interrupts masked. The events are typically generated from a
higher Exception level software, referred to as Dispatcher.
The event handler is executed at the client Exception level.

An event is either Shared or Private. Private events are per PE event and must
be registered and handled for each PE, who wish to participate in the SDEI
handling. Shared events are system wide events which are registered and handled
by a single PE according to the routing mode.

The events can be defined statically by the platform (platform event)
or dynamically created by the client for an interrupt (bound event). Event
number space is a 32bit signed number with standard event space and vendor
defined event space.

The SDEI instance between platform firmware and hypervisor client is referred
as the physical SDEI. The SDEI instance between hypervisor dispatcher
and operating system client is referred as virtual SDEI.

The SDEI specification defines a set of function calls that can be
used by a Non secure client to interact with the dispatcher to subscribe,
manage and handle the events. It defined two levels of event priority,
Normal and Critical. Both levels of events can preempt the client interrupt
handling.

Refer the [SDEI specification][1] for detailed description and client SDEI calls.

# Implementation
Please note that this is a prototype implementation and has undergone only
minimal testing at this stage. The function implementations can change
completely before the patch set gets upstreamed. However, the client SDEI call
signature will not change and is as per the specification.

The prototype implements the following:
 * Platform port for FVP platform.
 * Physical SDEI between firmware and hypervisor.
 * Interrupt based SDEI events.
 * Support 2 levels of priority for events.
 * Two bind slots for private events.
 * Two bind slots for shared events.
 * SDEI interrupts are configured as Group-0 Secure (EL3 interrupts).

The prototype does not implement:
 * Standard event 0, referred as signaling event.
 * SDEI calls SDEI_EVENT_SIGNAL API, SDEI_FEATURES, SDEI_EVENT_ROUTING_SET
 * PSCI integration.
 * Co-existence of secure payload like a Trusted OS.

## Exception class framework
For handling the G0S interrupts, the implementation make use of exception class
framework (referred as iclass). The iclass partitions, the G0S interrupts into
different levels of GIC priorities. Each subsystem which requires G0S
interrupts, registers an interrupt handler specifying the priority class. When
the handler for a particular priority class is running, the GIC sets the running
priority (ICC_RPR) to the respective priority level and disallows another same
or lower priority level handler to run.  The framework also supports
adding/removing interrupts into a priority class.

 SDEI implementation uses two priority classes: normal-SDEI and critical-SDEI.
 Critical-SDEI class is given higher priority over normal-SDEI class, therefore
 a critical priority SDEI event can preempt a normal priority SDEI event but
 a normal priority event cannot preempt another running normal event handler
 and a critical priority event cannot preempt another running critical event
 handler.

## Files and changes

The following files are added for the SDEI implementation:

`services/std_svc/sdei/sdei_main.c`:
This file implements the various SDEI client SMC entries.

`services/std_svc/sdei/sdei_intr_mgmt.c`:
This file implements the interrupt and context management of SDEI events.

`services/std_svc/sdei/sdei_event.c`:
This file implements helper functions to traverse the event definitions.

`plat/arm/board/fvp/fvp_sdei.c`:
This file includes the various event definitions supported by the platform. Any
new event definition should be added here. Refer to section [Adding Events].

`services/std_svc/sdei/sdei_pm.c`:
 This file implements the SDEI integration with power management operation.
 Currently this is a place holder and is not implemented.

`bl31/interrupt_class.c`:
 Interrupt class functions, used to partition G0S interrupts and register
 handlers for each class.

`include/services/sdei.h`:
 Main header file with SDEI function definitions.

`include/services/sdei_svc.h`:
 Header to facilitate integration with standard SMC calls.

`services/std_svc/sdei/sdei_private.h`:
 private header file for SDEI internal use.

`include/bl31/interrupt_class.h`:
 interrupt class management definitions.


# Porting
For porting this implementation to a different platform, the target
platform needs to:
  1. Define events, refer section on Defining events.
  2. Add the event file to compilation.

Once these changes are made, the build must be generated as explained in
the Build instructions section.


# Defining events

This prototype implementation of SDEI also includes a platform port for the ARM
FVP platform. The platform port defines SDEI events, its attributes, and how the
events map to platform interrupts.

The FVP platform port is contained in the file `plat/arm/board/fvp/fvp_sdei.c`.
It defines separate array for private and shared SDEI events:

```
static sdei_ev_map_t fvp_private_sdei[] = {
	...
};

static sdei_ev_map_t fvp_shared_sdei[] = {
	...
};
```

Finally, the SDEI global data is declared with the `DECLARE_SDEI_MAP` by passing
both the private and shared array. Note that `DECLARE_SDEI_MAP` has to be placed
in the same file as where the arrays are defined.

Individual private and share events are defined using the macros
`SDEI_PRIVATE_EVENT` and `SDEI_SHARED_EVENT`, respectively. They take the
following arguments:

  * SDEI event number being defined.
  * The platform event number that the SDEI event maps to. For those events that
    are bound at run time (using SDEI_INTERRUPT_BIND), the interrupt number
    shall be left as 0.
  * SDEI event attributes. Valid attributes can be found in
    `include/services/sdei.h`.

For example:

To define an event in standard event spare:
```
SDEI_PRIVATE_EVENT(8, 23, SDEI_MAPF_BOUND),
```

This defines the private SDEI event 8, and maps to interrupt 23, with the
attribute `SDEI_MAPF_SIGNALABLE`.


# Build instructions
To include the implementation in the build, add `SDEI_SUPPORT=1` to the build
command line.

# Known issues
 * The current implementation for SDEI_PE_MASK will disable
   the client interrupts by setting a high PMR value.
 * GICv2 is not supported
 * Bound interrupts are not allocated from vendor space.


[1]: http://infocenter.arm.com/help/topic/com.arm.doc.den0054a/ARM_DEN0054A_Software_Delegated_Exception_Interface.pdf "SDEI17"
