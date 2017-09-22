Exception Handling in ARM Trusted Firmware
==========================================


.. section-numbering::
    :suffix: .

.. contents::
    :depth: 2

This document describes various aspects of handling exceptions by Runtime
Firmware (BL31) that are targeted at EL3. In this document, exceptions mean
asynchronous events that happen in the system that alter the program flow. On
ARM systems, these include peripheral interrupts, External Aborts, System Error
(``SError``) interrupts.

ARM Trusted Firmware handles synchronous ``SMC`` exceptions raised from lower
ELs, but that's described in the `Firmware Design document`__.

.. __: firmware-design.rst#handling-an-smc

Introduction
------------

Through various control bits in the ``SCR_EL3`` register, the ARM architecture
allows for asynchronous exceptions to be routed to EL3. As described in the
`Interrupt Framework Design`_ document, depending on the chosen interrupt
routing model, ARM Trusted Firmware appropriately sets the ``FIQ`` and ``IRQ``
bits of ``SCR_EL3`` register. Other than the purpose of facilitating context
switch between Normal and Secure worlds, FIQs and IRQs routed to EL3 are not
normally required to handle in EL3.

However, the evolving system and standards landscape demands that various
exceptions are targeted at and handled in EL3. For instances:

-  Starting with ARMv8.2 architecture extension, many RAS features have been
   introduced to the ARM architecture. With RAS features implemented, various
   components of the system may use one of the asynchronous exceptions to signal
   error conditions to PEs. These error conditions are of critical nature, and
   it's imperative that corrective or remedial actions are taken at the earliest
   opportunity. Therefore, a *Firmware-first Handling* approach is generally
   followed in response to RAS events in the system.

-  The ARM `SDEI specification`_ defines interfaces through which Normal world
   interacts with the Runtime Firmware in order to request notification of
   system events. The SDEI specification requires that these events are notified
   even when the Normal world executes with the exceptions masked. This too
   implies that Firmware-first handling is required, where the events are first
   received by the EL3 firmware, and then dispatched to Normal world through
   purely software mechanism.

For ARM Trusted Firmware, Firmware-first handling means that asynchronous
exceptions are suitably routed to EL3, and the Runtime Firmware (BL31) is
extended to include software components that are capable of handling those
exceptions that target EL3. These components -- referred to as *dispatchers*
[#spd]_ in general -- may choose to:

-  Receive and handle exceptions entirely in EL3, meaning the exceptions
   handling terminates in EL3.

-  Receive, but handle part of the exception in EL3, and delegate rest of the
   handling to dedicated software stack running at lower Secure ELs. In this
   scheme, the handling spans various secure ELs.

-  Receive, but handle part of the exception in EL3, and delegate processing of
   the error to dedicated software stack running at lower secure ELs;
   additionally, the Normal world may also be required to participate in the
   handling, or be notified of such events (for example, as an SDEI event). In
   this scheme, exception handling potentially and maximally spans all ELs in
   both Secure and Normal worlds.

The |EHF| in ARM Trusted Firmware therefore aims to accommodate multiple
dispatchers, to provide mechanisms for dispatchers to be assigned priority, to
register exception handlers, and to validate delegation to lower exception
levels.

.. [#spd] Not to be confused with `Secure Payload Dispatcher`__, which is an
   EL3 component that operates in EL3 on behalf of Secure OS.

.. __: docs/firmware-design.rst#secure-el1-payloads-and-dispatchers

Interrupt handling
------------------

The |EHF| always routes *Group 0* interrupts to EL3 by setting ``SCR_EL3.FIQ``;
i.e. all ``FIQs`` target EL3 both when executing in Secure and Normal world. It
also registers the top-level handler for interrupts that target EL3, as
described in the `Interrupt Framework Design`_ document.

EL3 dispatchers may chose to receive and handle interrupts. The platform assigns
a group of interrupts to individual dispatchers by assigning distinct priority
range. This means all interrupts that target a given dispatcher falls in a
certain priority range, which would be different to the priority range of
interrupts targeting another dispatcher. Interrupt priority, therefore,
identifies the dispatcher that handles an interrupt received at EL3.

Individual dispatchers are assigned interrupt priority ranges in two steps:

Associating priority
~~~~~~~~~~~~~~~~~~~~

Interrupts are associated to dispatchers by way of grouping and assigning
interrupts to a priority range. In other words, all interrupts that are to
target a particular dispatcher should fall in a particular priority range. For
priority assignment:

-  Of the 8 bits of priority that ARM GIC architecture permits, bit 7 must be 0
   (secure space).

-  Depending on the number of dispatchers to support, the platform must choose
   to use *n* of 7 remaining bits to identify and assign interrupts to
   individual dispatchers. Choosing *n* bits supports up to 2\ :sup:`n`-1
   distinct dispatchers. For example, by choosing 2 additional bits, the
   platform can accommodate up to 3 secure priority ranges to dispatchers:
   ``0x20``, ``0x40``, and ``0x60``.

Note:

   The ARM GIC architecture requires that a GIC implementation that supports two
   security states must implement at least 32 priority levels, meaning, at least
   5 upper bits of the 8 bits are writeable. In the scheme described above, when
   choosing *n* bits for priority range assignment, the platform must ensure
   that at least ``n+1`` top bits of GIC priority are writeable.

.. _exception-descriptor:

Having chosen priority ranges for individual dispatchers, the platform expresses
interrupt priority range assignments to dispatches by declaring an array of
descriptors. Each descriptor in the array is of type ``exc_pri_desc_t``, and
describes the association of interrupt priority range to a dispatcher (its
handler), and shall be populated by the ``EXC_INSTALL_DESC()`` macro.

Note:

   The macro ``EXC_INSTALL_DESC()`` installs the descriptors in the array at a
   computed index, and not necessarily where the macro is placed in the array.
   The size of the array might therefore be larger than what it appears to be.
   The ``ARRAY_SIZE()`` macro therefore should be used to determine the size of
   array.

Finally, this array of descriptors is exposed to |EHF| via. the
``DECLARE_EXCEPTIONS()`` macro.

Refer to the `Interrupt handling example`_. See also: `Interrupt Prioritisation
Considerations`_.

Programming priority
~~~~~~~~~~~~~~~~~~~~

The `Firmware Design guide`__ explains methods for configuring secure
interrupts. |EHF| requires that secure interrupts and their respective
properties are specified as described therein. The priority of secure interrupts
determined in the `Associating priority`_ section must be listed accordingly in
the array of secure interrupts properties.

.. __: firmware-design.rst#configuring-secure-interrupts

See `Limitations`_, and also refer to `Interrupt handling example`_ for
illustration..

Registering handler
~~~~~~~~~~~~~~~~~~~

There are two ways to register an interrupt handler for a priority range:

-  Specify the interrupt handler as an argument to ``EXC_INSTALL_DESC()`` macro.
   The handler will be associated at build time to the priority range being
   specified.

-  Through the API ``exc_register_priority_handler``. The API takes two
   arguments:

   -  The priority range for which the handler is being registered;

   -  The handler to be registered.

The API will succeed, and return ``0``, only if:

-  There exists a descriptor with the priority range requested.

-  There are no handlers registered already, either at build-time, or by a
   previous call to the API.

Otherwise, the API returns ``-1``.

The interrupt handler ought to have the following signature:

.. code:: c

   typedef int (*exc_handler_t)(struct exc_pri_desc *desc, uint32_t intr,
        uint32_t flags, void *handle, void *cookie);

The ``desc`` parameter is the pointer to the descriptor belonging to the
priority range. Rest of the parameters are as passed to the top-level EL3
interrupt handler; see `Interrupt handling`_.

By the time a handler is called, that priority range is said to be *active*. See
`Activating, Deactivating, and Querying priorities`_.

Interrupt handling example
~~~~~~~~~~~~~~~~~~~~~~~~~~

The following annotated snippet demonstrates how a platform might choose to
assign interrupts to fictitious dispatchers:

.. code:: c

   #include <exception_mgmt.h>
   #include <gic_common.h>
   #include <interrupt_mgmt.h>

   ...

   /* This platform uses 2 bits for interrupt association */
   #define PLAT_PRI_BITS        2

   /* Priorities for individual dispatchers */
   #define DISP1_PRIO           0x20
   #define DISP2_PRIO           0x40
   #define DISP3_PRIO           0x60

   /* Install descriptors for all dispatchers */
   exc_pri_desc_t plat_exceptions[] = {
        EXC_INSTALL_DESC(PLAT_PRI_BITS, DISP1_PRIO, disp1_handler),
        EXC_INSTALL_DESC(PLAT_PRI_BITS, DISP2_PRIO, disp2_handler),
        EXC_INSTALL_DESC(PLAT_PRI_BITS, DISP3_PRIO, disp3_handler),
   };

   /* Expose priority descriptors to Exception Handling Framework */
   DECLARE_EXCEPTIONS(plat_exceptions, ARRAY_SIZE(plat_exceptions),
        PLAT_PRI_BITS);

   ...

   /* List interrupt properties for GIC driver. All interrupts target EL3 */
   const interrupt_prop_t plat_interrupts[] = {
        /* Interrupts for dispatcher 1 */
        INTR_PROP_DESC(d1_0, DISP1_PRIO, INTR_TYPE_EL3, INTR_CFG_LEVEL),
        INTR_PROP_DESC(d1_1, DISP1_PRIO, INTR_TYPE_EL3, INTR_CFG_LEVEL),

        /* Interrupts for dispatcher 2 */
        INTR_PROP_DESC(d2_0, DISP2_PRIO, INTR_TYPE_EL3, INTR_CFG_LEVEL),
        INTR_PROP_DESC(d2_1, DISP2_PRIO, INTR_TYPE_EL3, INTR_CFG_LEVEL),

        /* Interrupts for dispatcher 3 */
        INTR_PROP_DESC(d3_0, DISP3_PRIO, INTR_TYPE_EL3, INTR_CFG_LEVEL),
        INTR_PROP_DESC(d3_1, DISP3_PRIO, INTR_TYPE_EL3, INTR_CFG_LEVEL),
   };

See also the `Build-time flow`_ and the `Run-time flow`_.

Activating, Deactivating, and Querying priorities
-------------------------------------------------

A priority range is said to be *active* when an interrupt of that priority is
being handled. In other words, by the time a dispatcher's interrupt handler
called from the |EHF|, the dispatcher's priority range is marked active.

When the dispatcher has reached a logical resolution for the cause of the
interrupt, it has to explicitly *deactivate* the priority level. There are
potentially two work flows to this effect:

.. _deactivation workflows:

-  The dispatcher has addressed the cause of the interrupt, and had decided to
   take no further action. In this case, the dispatcher's handler deactivates
   the priority range before returning to the |EHF|. Runtime firmware, upon exit
   through an ``ERET``, resumes execution before the interrupt occurred.

-  The dispatcher has to delegate the execution to lower ELs, and the cause of
   the interrupt can be considered resolved only when the lower EL returns
   signals complete (via. an ``SMC``) at a future point in time. The following
   sequence ensues:

   #. The dispatcher sets up the next context for delegating to a lower EL upon
      exiting EL3 through an ``ERET``.

   #. The dispatcher returns to the |EHF| without deactivating priority range;
      the priority range remains *active*.

   #. When the runtime firmware exits with an ``ERET``, a delegated lower EL is
      entered.

   #. The lower EL completes its execution, and signals via. an ``SMC``.

   #. The ``SMC`` is handled by the same dispatcher that handled the interrupt
      previously. It amends the next context to resume the interrupted state,
      deactivates the priority range, and returns.

   #. Runtime firmware, upon exit through an ``ERET``, resumes execution before
      the interrupt occurred.

In order to facilitate the aforementioned work flows, the |EHF| provides the
following APIs:

-  ``exc_current_priority()`` retrieves the currently active priority range, or
   ``-1`` if none is active. Dispatcher handlers won't observe the API returning
   ``-1`` as, by the time dispatcher handler executes, the corresponding
   priority level is already *active*.

-  ``exc_activate_priority()`` activates a given priority only if the current
   active priority is less than the given one; otherwise panics.

-  ``exc_deactivate_priority()`` deactivates a given priority only if the
   current active priority is equal to the given one; otherwise panics.

See also the `Run-time flow`_.

Interrupt Prioritisation Considerations
---------------------------------------

The GIC priority scheme, by design, prioritises Secure interrupts over Normal
world ones. The |EHF| further assigns relative priorities amongst Secure
dispatchers.

As mentioned in `Interrupt handling`_, interrupts targeting distinct dispatchers
fall in distinct priority ranges. Because they're routed via. the GIC, interrupt
delivery to the PE is subject to GIC prioritisation rules. In particular, when
an interrupt is being handled by the PE (i.e., the interrupt is in *Active*
state), only interrupts of higher priority are signalled to the PE, even if
interrupts of same or lower priority are pending. This has the side effect of
one dispatcher being starved of interrupts by virtue of another dispatcher
handling its (higher priority) interrupts.

The |EHF| doesn't enforce a particular prioritisation policy, but the platform
should carefully consider the assignment of priorities to dispatchers integrated
into runtime firmware. The platform should sensibly delineate priority to
various dispatchers according to their nature. In particular, dispatchers of
critical nature (RAS, for example) should be assigned higher priority than
others (SDEI, for example); and within SDEI, Critical priority SDEI are assigned
higher priority than Normal ones.

Build-time flow
---------------

The build-time flow involves the following steps:

#. Platform assigns priorities by installing exception descriptors for
   individual dispatchers, as described in `Associating priority`_.

#. Platform providing interrupt properties to GIC driver, as described in
   `Programming priority`_.

#. Unless a handler was registered within the `exception descriptors`__, call
   ``exc_register_priority_handler()`` to install an interrupt handler.

   .. __: `exception-descriptor`_

Also refer to the `Interrupt handling example`_.

Run-time flow
-------------

#. The GIC driver, during initialization, iterates through the platform-supplied
   interrupt properties (see `Programming priority`_), and configures the
   interrupts. This programs the appropriate priority and group (Group 0) on
   interrupts belonging to different dispatchers.

#. The |EHF|, during its initialisation, registers a top-level interrupt handler
   with the `Interrupt Management Framework`__ for EL3 interrupts. This also
   results in setting ``SCR_EL3.FIQ``.

   .. __: `Interrupt Framework Design`_

#. When an interrupt belonging to a dispatcher fires, GIC raises an ``FIQ``, and
   is taken to EL3.

#. The top-level EL3 interrupt handler executes. The handler Acknowledges the
   interrupt, reads its *Running Priority*, and from that, determines the
   dispatcher handler.

#. The |EHF| marks that priority range *active*, and jumps to the dispatcher
   handler.

#. Once the dispatcher handler finishes its job, it has to either immediately
   *deactivate* the priority range before returning to the |EHF|, or defer it
   until a delegation completes. See `deactivation workflows`_.

Limitations
-----------

The |EHF| has the following limitations:

-  Although there could be up to 126 secure dispatchers supported by the GIC
   priority scheme, the size of descriptor array exposed with
   ``DECLARE_EXCEPTIONS()`` macro is currently limited to 32. This serves most
   expected use cases. This may be expanded in the future, should use cases
   demand so.

-  The platform must ensure that that the priority assigned to the dispatcher in
   the exception descriptor, and the programmed priority of interrupts handled
   by the dispatcher, match. The |EHF| cannot verify that this has been
   followed.

----

*Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.*

.. |EHF| replace:: *Exception Handling Framework*

.. _Interrupt Framework Design: interrupt-framework-design.rst
.. _SDEI specification: http://infocenter.arm.com/help/topic/com.arm.doc.den0054a/ARM_DEN0054A_Software_Delegated_Exception_Interface.pdf
