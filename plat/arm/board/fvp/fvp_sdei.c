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

/* SDEI mappings for FVP */

#include <sdei.h>

/*
 * Mapping table provides mapping of event to interrupts and the properties of
 * the map like private, dynamic event etc. The mapping table for private and
 * shared are kept separate to simplify implementation. Each private/shared map
 * will have a corresponding event entry in the same array offset in
 * private/shared respectively. They are again seperated as it allows to avoid
 * false sharing and better locking.
 */
static sdei_ev_map_t fvp_private_sdei[] = {
	/* Private event mappings */
	SDEI_PRIVATE_EVENT(0, 8, SDEI_MAPF_SIGNALABLE),

	/* PMU interrupt */
	SDEI_PRIVATE_EVENT(8, 23, SDEI_MAPF_BOUND),

	/* Dynamic private events */
	SDEI_PRIVATE_EVENT(100, 0, SDEI_MAPF_DYNAMIC),
	SDEI_PRIVATE_EVENT(101, 0, SDEI_MAPF_DYNAMIC)
};

/* Shared event mappings */
static sdei_ev_map_t fvp_shared_sdei[] = {
	/*
	 * Int
	 * 32	Watchdog, SP805
	 * 34	Dual-Timer 0, SP804
	 * 35	Dual-Timer 1, SP804
	 *
	 * Memory:
	 * Watchdog, SP805	0x00_1C0F_0000	64KB	0x00_1C0F_FFFF
	 * Dual-Timer 0, SP804	0x00_1C11_0000	64KB	0x00_1C11_FFFF
	 * Dual-Timer 1, SP804	0x00_1C12_0000	64KB	0x00_1C12_FFFF
	 */

	/* SP804 Timer 0 FVP timer */
	SDEI_SHARED_EVENT(804, 0, SDEI_MAPF_DYNAMIC),

	/* SP804 Timer 1 FVP timer */
	SDEI_SHARED_EVENT(1804, 35, SDEI_MAPF_BOUND),

	/* Dynamic shared mapping */
	SDEI_SHARED_EVENT(3000, 0, SDEI_MAPF_DYNAMIC),
	SDEI_SHARED_EVENT(3001, 0, SDEI_MAPF_DYNAMIC)
};

DECLARE_SDEI_MAP(fvp_private_sdei, fvp_shared_sdei);
