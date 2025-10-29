#ifndef FOWLER_NOLL_VO_1A_32_BIT_HASH
#define FOWLER_NOLL_VO_1A_32_BIT_HASH

#include "compiler_attributes_macros.h"

#include <stddef.h>  // size_t
#include <stdint.h>  // uint32_t

/*! @brief FNV-0 and FNV-1 32 bit magic prime. */
#define FNV_32_PRIME ((uint32_t)0x01000193U)
/*!
 * @brief FNV-1 and FNV-1a 32 bit non-zero initial basis.
 * FNV-0 uses 0 as the the initial value.
 */
#define FNV1_32_INIT ((uint32_t)0x811c9dc5U)

static void fnv32a_hash(
	const unsigned char *const restrict buffer, const size_t len,
	void *const restrict out
) _nonnull;

/*!
 * @brief perform a 32 bit Fowler/Noll/Vo FNV-1a hash on a buffer.
 *
 * @param buffer start of buffer to hash.
 * @param len length of buffer in octets.
 * @param out pointer to an initialised 4 byt wide address to store the hash.
 *
 * `out` should be initialised to `FNV1_32_INIT` or the previous hash.
 *
 * @returns	32 bit hash as a static hash type.
 */
static void fnv32a_hash(
	const unsigned char *const buffer, const size_t len,
	void *const restrict out
)
{
	const unsigned char *const buf_end = buffer + len;
	uint32_t *const restrict dest = out;

	for (const unsigned char *buf_p = buffer; buf_p < buf_end; buf_p++)
	{
		/* xor the bottom with the current octet */
		*dest ^= *buf_p;
		/* multiply by the 32 bit FNV magic prime mod 2^32 */
		*dest *= FNV_32_PRIME;
	}
}

#undef FNV_32_PRIME
#undef FNV1_32_INIT

#endif /* FOWLER_NOLL_VO_1A_32_BIT_HASH */
