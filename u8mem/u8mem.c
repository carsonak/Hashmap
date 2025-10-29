#define _ISOC99_SOURCE /* snprintf */

#include <limits.h>
#include <stddef.h> /* size_t */
#include <stdio.h>  /* snprintf */
#include <string.h> /* memcpy */

#include "u8mem.h"
#include "xalloc/xalloc.h"

/*!
 * @brief allocate and initialise memory for a `u8mem` type.
 *
 * @param mem array of bytes to initialise with.
 * @param len length of the array of bytes.
 * @returns pointer to the initialised u8mem, NULL on failure.
 */
u8mem *u8mem_new(const unsigned char *const restrict mem, const len_ty len)
{
	if (len < 1)
		return (NULL);

	const size_t mem_size = sizeof(unsigned char) * len;

	/* overflow error. */
	if (SIZE_MAX - mem_size < sizeof(u8mem))
		return (NULL);

	u8mem *const restrict m = xmalloc(sizeof(*m) + mem_size);

	if (!m)
		return (NULL);

	*m = (u8mem){.len = len, .buf = (unsigned char *)(m + 1)};
	if (mem)
		memcpy(m->buf, mem, len);

	return (m);
}

/*!
 * @brief free memory of a `u8mem` type.
 *
 * @param m pointer to the u8mem to free.
 */
void *u8mem_delete(u8mem *const restrict m)
{
	if (m)
		*m = (u8mem){0};

	return (xfree(m));
}

/*!
 * @brief compare two memory blocks.
 *
 * @param m_a the first memory block.
 * @param m_b the second memory block.
 * @returns 0 if both blocks are equal,
 * `m_a.len - m_b.len` or results of a memcmp of `m_a` and `m_b`.
 */
int u8mem_compare(const u8mem m_a, const u8mem m_b)
{
	return (m_a.len - m_b.len || memcmp(m_a.buf, m_b.buf, m_a.len));
}

/*!
 * @brief return the string representation of a `u8mem` type.
 *
 * @param m the u8mem to stringify.
 * @returns pointer to the string, NULL on failure.
 */
char *u8mem_tostr(const u8mem m)
{
	const int s_len =
		snprintf(NULL, 0, "(%" PRI_len ", %p)", m.len, (void *)m.buf);

	if (s_len < 0)
		return (NULL);

	char *const restrict s = xmalloc(sizeof(*s) * (s_len + 1));

	if (s)
		snprintf(s, s_len + 1, "(%" PRI_len ", %p)", m.len, (void *)m.buf);

	return (s);
}
