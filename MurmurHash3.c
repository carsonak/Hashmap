#ifndef MURMURHASH3_X86_32_HASH
#define MURMURHASH3_X86_32_HASH

// murmurhash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

// Note - The x86 and x64 versions do _not_ produce the same results, as the
// algorithms are optimized for their respective platforms. You can still
// compile and run any of them on any platform, but your performance with the
// non-native version will be less than optimal.

#include "compiler_attributes_macros.h"

#include <stddef.h>  // size_t
#include <stdint.h>
#include <string.h>  // memcpy

#ifdef __GNUC__
	#define FORCE_INLINE __attribute__((always_inline)) inline
#else
	#define FORCE_INLINE inline
#endif

static FORCE_INLINE uint32_t rotl32(uint32_t x, int8_t r)
{
	return (x << r) | (x >> (32 - r));
}

/*!
 * @brief Read 4 bytes from pointer.
 *
 * If your platform needs to do endian-swapping or can only handle aligned reads,
 * do the conversion here.
 */
FORCE_INLINE uint32_t getblock32(const uint8_t *const restrict p)
{
	uint32_t d;
	memcpy(&d, p, sizeof(d));
	return d;
}

/*! @brief Finalization mix - force all bits of a hash block to avalanche. */
static FORCE_INLINE uint32_t fmix32(uint32_t h)
{
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

static void murmurhash3_x86_32(
	const unsigned char *const restrict key, const size_t len,
	void *const restrict out
) _nonnull;

static void murmurhash3_x86_32(
	const unsigned char *const restrict key, const size_t len,
	void *const restrict out
)
{
	uint32_t *const restrict dest = out;
	const size_t nblocks = len / 4;
	const uint32_t c1 = 0xcc9e2d51, c2 = 0x1b873593;
	uint32_t hash = *dest;

	for (unsigned int i = 0; i < nblocks; i++)
	{
		uint32_t k1 = getblock32(&key[i * 4]);

		k1 = rotl32(k1 * c1, 15) * c2;

		hash = rotl32(hash ^ k1, 13);
		hash = hash * 5 + 0xe6546b64;
	}

	const uint8_t *const tail = key + nblocks * 4;
	uint32_t k1 = 0;

	switch (len & 3)
	{
		/* clang-format off */
	case 3: k1 ^= tail[2] << 16;
	case 2: k1 ^= tail[1] << 8;
	case 1: k1 ^= tail[0] << 0;
		/* clang-format on */
		k1 = rotl32(k1 * c1, 15) * c2;
		hash ^= k1;
	};

	hash ^= len;
	hash = fmix32(hash);

	*dest = hash;
}

#undef FORCE_INLINE

#endif  // MURMURHASH3_X86_32_HASH
