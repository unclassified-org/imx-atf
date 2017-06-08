#
# Copyright (c) 2013-2017, ARM Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include lib/psci/psci_lib.mk

BL31_SOURCES		+=	bl31/bl31_main.c				\
				bl31/exception_mgmt.c				\
				bl31/interrupt_mgmt.c				\
				bl31/aarch64/bl31_entrypoint.S			\
				bl31/aarch64/runtime_exceptions.S		\
				bl31/aarch64/crash_reporting.S			\
				bl31/bl31_context_mgmt.c			\
				common/runtime_svc.c				\
				plat/common/aarch64/platform_mp_stack.S		\
				services/std_svc/std_svc_setup.c		\
				${PSCI_LIB_SOURCES}

ifeq (${ENABLE_PMF}, 1)
BL31_SOURCES		+=	lib/pmf/pmf_main.c
endif

ifeq (${SDEI_SUPPORT},1)
ifeq (${ARCH},aarch32)
  $(error SDEI is only supported for AArch64)
endif
BL31_SOURCES		+=	services/std_svc/sdei/sdei_event.c	\
				services/std_svc/sdei/sdei_intr_mgmt.c	\
				services/std_svc/sdei/sdei_main.c
endif

BL31_LINKERFILE		:=	bl31/bl31.ld.S

# Flag used to indicate if Crash reporting via console should be included
# in BL31. This defaults to being present in DEBUG builds only
ifndef CRASH_REPORTING
CRASH_REPORTING		:=	$(DEBUG)
endif

$(eval $(call assert_boolean,CRASH_REPORTING))
$(eval $(call add_define,CRASH_REPORTING))
