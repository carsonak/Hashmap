#ifndef POWER_OF_2_ROUNDUP
#define POWER_OF_2_ROUNDUP

#include <limits.h> /* CHAR_BIT */
#include <stddef.h> /* size_t */

/*!
 * @brief round up a number to the next power of 2.
 *
 * @param n the number to round up.
 * @returns next power of 2 greater or equal to `n`.
 */
static size_t power2_roundup(size_t n)
{
	n--;
	for (size_t i = 1; i < sizeof(n) * CHAR_BIT; i *= 2)
	{
		n |= n >> i;
	}

	return ++n;
}

#endif /* POWER_OF_2_ROUNDUP */
