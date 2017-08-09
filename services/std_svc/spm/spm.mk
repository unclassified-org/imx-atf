#
# Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
#
# Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# Neither the name of ARM nor the names of its contributors may be used
# to endorse or promote products derived from this software without specific
# prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

ifneq (${SPD},none)
        $(error "Error: SPD and SPM are incompatible build options.")
endif
ifneq (${ARCH},aarch64)
        $(error "Error: SPM is only supported on aarch64.")
endif
ifneq (${ARM_BL31_IN_DRAM}, 1)
        $(error "Error: SPM is only supported when BL31 is in Secure DRAM.")
endif
ifneq (${PLAT}, fvp)
        $(error "Error: SPM is only supported on the ARM Base FVP")
endif

# SPM sources


SPM_SOURCES	:=	$(addprefix services/std_svc/spm/,	\
			spm_main.c				\
			spm_pwr_mgmt.c				\
			${ARCH}/spm_helpers.S			\
			secure_partition_setup.c		\
			secure_partition_xlat_tables.c		\
			${ARCH}/secure_partition_exceptions.S)


# Let the top-level Makefile know that we intend to include a BL32 image
NEED_BL32		:=	yes

ifneq (,${SEC_PART})
        ifneq (,${BL32})
                $(error "Prebuilt Secure Partition cannot be specified in option BL32 at the same time as building a Secure Partition from sources (SEC_PART option)")
        endif

        # Secure Partition image sources
        SEC_PART_MAKE := $(wildcard bl32/${SEC_PART}/${SEC_PART}.mk)
        ifeq (${SEC_PART_MAKE},)
	        $(error Error: No bl32/${SEC_PART}/${SEC_PART}.mk located)
        endif
        include bl32/${SEC_PART}/${SEC_PART}.mk
endif
