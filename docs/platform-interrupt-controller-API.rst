Platform Interrupt Controller API documentation
===============================================

.. section-numbering::
    :suffix: .

.. contents::

This document lists the platform interrupt controller API that abstracts the
runtime configuration and control of interrupt controller from the generic
code.

Function: unsigned int plat_ic_get_running_priority(void); [optional]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    Argument : void
    Return   : unsigned int

This API should return the priority of the interrupt the PE is currently
servicing. This must be be called only after an interrupt has already been
acknowledged via. ``plat_ic_acknowledge_interrupt``.

In the case of ARM standard platforms using GIC, the *Running Priority Register*
is read to determine the priority of the interrupt.
