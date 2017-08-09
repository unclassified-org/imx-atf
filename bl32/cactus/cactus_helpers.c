/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>

/* From FreeBSD */
static unsigned int rand_next = 1;

/** Compute a pseudo-random number.
 *
 * Compute x = (7^5 * x) mod (2^31 - 1)
 * without overflowing 31 bits:
 *      (2^31 - 1) = 127773 * (7^5) + 2836
 * From "Random number generators: good ones are hard to find",
 * Park and Miller, Communications of the ACM, vol. 31, no. 10,
 * October 1988, p. 1195.
 **/
int
rand()
{
	int hi, lo, x;

	/* Can't be initialized with 0, so use another value. */
	if (rand_next == 0)
		rand_next = 123459876;
	hi = rand_next / 127773;
	lo = rand_next % 127773;
	x = 16807 * lo - 2836 * hi;
	if (x < 0)
		x += 0x7fffffff;
	return ((rand_next = x) % ((unsigned int)RAND_MAX + 1));
}


uintptr_t bound_rand(uintptr_t min, uintptr_t max)
{
	/*
	 * This is not ideal as some numbers will never be generated because of
	 * the integer arithmetic rounding.
	 */
	return ((rand() * (UINT64_MAX/INT_MAX)) % (max - min)) + min;
}


void expect(int expr, int expected)
{
	if (expr != expected) {
		ERROR("Expected value %i, got %i\n", expected, expr);
		while (1)
			continue;
	}
}
