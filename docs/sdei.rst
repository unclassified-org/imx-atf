Software Delegated Exception Interface
======================================


.. section-numbering::
    :suffix: .

.. contents::
    :depth: 2

This document discusses the implementation of SDEI dispatcher in ARM Trusted
Firmware.

Introduction
------------

`Software Delegated Exception Interface`__ (SDEI) is an ARM published
specification for software executing in lower ELs (EL2 and below, referred to as
*clients*) to register handlers with the firmware in order to receive
notification of system events. Clients thus receive the event notification at
the registered handler, even when the client executes with its exceptions
masked. The list of SDEI events available are specific to the platform
[#std-event]_.

This document henceforth assumes that the reader is familiar with the SDEI
specification, interfaces, and their requirements, and only discusses the
design and implementation of SDEI dispatcher in ARM Trusted Firmware.

.. __: `SDEI specification`_
.. [#std-event] Except event 0, defined by the SDEI specification.

Defining events
---------------

The platform is expected to provide two arrays of event descriptors: one for
private events, and another for shared. The SDEI dispatcher provides
``SDEI_PRIVATE_EVENT()`` and ``SDEI_SHARED_EVENT()`` macros to populate the
event descriptors. Both macros take 3 arguments:

-  The event number, which is an unsigned 32-bit integer.

-  The interrupt number the event is bound to. If it's not applicable to this
   event, or if the event is dynamic, leave this as ``0``.

-  A bit map of `Event flags`_.

Once the event arrays are defined, they should be exported to the SDEI
dispatcher using the ``DECLARE_SDEI_MAP()`` macro, passing it the pointers to
the private and shared event arrays, respectively.

Also refer to the `Example`_.

Event flags
~~~~~~~~~~~

Event flags describe properties of the event. They are bit maps that can be
``OR``\ ed to form parameters to macros that `defines events`__.

.. __: `Defining events`_

-  ``SDEI_MAPF_DYNAMIC``: Marks the event as dynamic. Dynamic events can be
   bound to any non-secure interrupt at runtime via. the ``SDEI_INTERRUPT_BIND``
   and ``SDEI_INTERRUPT_RELEASE`` calls.

-  ``SDEI_MAPF_BOUND``: Marks the event as permanently bound to an interrupt.
   These events cannot be re-bound at run time.

-  ``SDEI_MAPF_SIGNALABLE``: Special flag reserved only for event 0. There must
   be exactly one event with this flag defined on the platform.

-  ``SDEI_MAPF_CRITICAL``: Marks the vent has having *Critical* priority.
   Without this flag, the event is assumed to have Normal priority.

Example
-------

.. code:: c

   static sdei_ev_map_t plat_private_sdei[] = {
        /* Use SGI for event 0 */
        SDEI_PRIVATE_EVENT(0, 8, SDEI_MAPF_SIGNALABLE),

        /* PPI */
        SDEI_PRIVATE_EVENT(8, 23, SDEI_MAPF_BOUND),

        /* Dynamic private events */
        SDEI_PRIVATE_EVENT(100, 0, SDEI_MAPF_DYNAMIC),
        SDEI_PRIVATE_EVENT(101, 0, SDEI_MAPF_DYNAMIC)
   };

   /* Shared event mappings */
   static sdei_ev_map_t plat_shared_sdei[] = {
        SDEI_SHARED_EVENT(804, 0, SDEI_MAPF_DYNAMIC),

        /* Dynamic shared events */
        SDEI_SHARED_EVENT(3000, 0, SDEI_MAPF_DYNAMIC),
        SDEI_SHARED_EVENT(3001, 0, SDEI_MAPF_DYNAMIC)
   };

   /* Export SDEI events */
   DECLARE_SDEI_MAP(plat_private_sdei, plat_shared_sdei);

Interaction with Exception Handling Framework
---------------------------------------------

The SDEI dispatcher functions alongside the `Exception Handling Framework`_.
This means that the platform must assign a priority the SDEI dispatcher, and
must additionally:

-  Populate `exception descriptors`__ for the SDEI dispatcher's interrupt
   handler. The SDEI dispatcher's interrupt handler (both Critical and Normal
   priorities) is ``sdei_intr_handler``.

   .. __: exception-handling.rst#associating-priority

-  List all `interrupt properties`__ corresponding to the SDEI events that have
   ``SDEI_MAPF_BOUND`` flag, so that the GIC driver programs the right priority.

   .. __: exception-handling.rst#programming-priority

Implementation specifics
------------------------

This section describes the dispatcher aspects that are specific to ARM Trusted
Firmware.

PE_MASK/PE_UNMASK call
~~~~~~~~~~~~~~~~~~~~~~

According to the SDEI specification, a PE comes out of reset with the events
masked. The client therefore is expected to call ``PE_UNMASK`` to unmask SDEI
events on the PE.

Validating handlers
~~~~~~~~~~~~~~~~~~~

The SDEI implementation uses a weak function
``plat_validate_sdei_entry_point()`` to validate the address of handlers
registered for SDEI events. The function takes one argument, which is the
address of the handler the SDEI client requested to register. The function must
return ``0`` for successful validation, or any non-zero value upon failure.

The default implementation always returns true. However, platforms can re-define
the function to implement a validation mechanism of its choice.

----

*Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.*

.. _SDEI specification: http://infocenter.arm.com/help/topic/com.arm.doc.den0054a/ARM_DEN0054A_Software_Delegated_Exception_Interface.pdf
.. _Exception Handling Framework: exception-handling.rst
