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

#include "sdei_private.h"
#include <utils.h>

#define ARRAY_OFF(_elem, _array) (int)((_elem) - (_array))

/*
 * Get SDEI entry with the given mapping: On success, returns pointer to SDEI
 * entry. On error, returns NULL.
 */
sdei_entry_t *get_event_entry(sdei_ev_map_t *map)
{
	const sdei_mapping_t *mapping;
	int idx;

	if (is_event_private(map)) {
		sdei_ev_map_t *this_base;

		/* Base of private mappings for this CPU */
		mapping = &sdei_global_mappings[SDEI_MAP_IDX_PRIV];
		this_base = &mapping->map[this_cpu() * mapping->size];
		idx = ARRAY_OFF(map, this_base);
		return &sdei_private_event_table[idx];
	} else {
		mapping = &sdei_global_mappings[SDEI_MAP_IDX_SHRD];
		idx = ARRAY_OFF(map, mapping->map);
		return &sdei_shared_event_table[idx];
	}
}

/*
 * Find event mapping for a given interrupt number: On success, returns pointer
 * to the event mapping. On error, returns NULL.
 */
sdei_ev_map_t *find_event_map_by_intr(uint32_t intr_num, int shared)
{
	sdei_ev_map_t *map;
	int i;

	for_each_shared_map(i, map) {
		if (map->intr == intr_num)
			return map;
	}

	return NULL;
}

/*
 * Find event mapping for a given event number: On success returns pointer to
 * the event mapping. On error, returns NULL.
 */
sdei_ev_map_t *find_event_map(uint32_t ev_num)
{
	const sdei_mapping_t *mapping;
	sdei_ev_map_t *map;
	unsigned int i, j;

	for_each_mapping(i, mapping) {
		iterate_mapping(mapping, j, map) {
			if (map->ev_num == ev_num)
				return map;
		}
	}

	return NULL;
}
