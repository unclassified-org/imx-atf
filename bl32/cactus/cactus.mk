#
# Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

INCLUDES	+=	-I bl32/cactus					\
			-I include/services

BL32_SOURCES	:=	bl32/cactus/aarch64/cactus_entrypoint.S		\
			bl32/cactus/aarch64/cactus_arch_helpers.S	\
			bl32/cactus/cactus_main.c			\
			bl32/cactus/cactus_helpers.c			\
			bl32/cactus/cactus_tests_memory_attributes.c	\
			plat/common/${ARCH}/platform_up_stack.S

BL32_LINKERFILE	:=	bl32/cactus/cactus.ld.S

SPD_SOURCES	+=	bl32/cactus/cactus_mappings.c

$(eval $(call add_define,CACTUS))
