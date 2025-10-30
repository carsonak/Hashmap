/* bench_hashmap.c
 *
 * Micro-benchmark simulating how a toy-language compiler might use the HashMap:
 *  - Insert many symbol table entries (identifiers) during parsing.
 *  - Perform many lookups (name resolution / type lookup).
 *  - Emulate scope entry/exit by inserting and removing short-lived entries.
 *
 * Example usage (after building):
 *   ./bench_hashmap --ops 500000 --seed 1234 --ratio 0.7 --scopes 1000
 *
 */

#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <valgrind/callgrind.h>

#include "HashMap.h"

/* Simple string key helper */
static struct u8mem *mkkey_from_uint(char *buf, size_t buflen, unsigned v)
{
	int n = snprintf(buf, buflen, "sym_%08x", v);
	return u8mem_new((unsigned char *)buf, (size_t)n);
}

/* RNG helpers */
static inline uint32_t xorshift32(uint32_t *s)
{
	uint32_t x = *s;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*s = x;
	return x;
}

int main(int argc, char **argv)
{
	size_t N_ops = 200000; /* total operations */
	uint32_t seed = 42;
	double insert_ratio = 0.6; /* fraction of operations that are inserts */
	size_t initial_capacity = 1021;
	int opt;

	while ((opt = getopt(argc, argv, "n:s:r:c:")) != -1)
	{
		switch (opt)
		{
		case 'n':
			N_ops = (size_t)atoll(optarg);
			break;
		case 's':
			seed = (uint32_t)atoi(optarg);
			break;
		case 'r':
			insert_ratio = atof(optarg);
			break;
		case 'c':
			initial_capacity = (size_t)atoi(optarg);
			break;
		default:
			break;
		}
	}

	/* Create the map */
	struct HashMap_int *hm = hm_int_new(initial_capacity);
	if (!hm)
	{
		fprintf(stderr, "failed to create hashmap\n");
		return 2;
	}

	uint32_t rng = seed;
	const size_t keybuf_len = 64;
	char keybuf[keybuf_len];

	/* Warm-up: insert initial keys to populate the table a bit */
	for (size_t i = 0; i < initial_capacity / 2; ++i)
	{
		struct u8mem *k = mkkey_from_uint(keybuf, keybuf_len, i);
		// CALLGRIND_START_INSTRUMENTATION;
		// CALLGRIND_TOGGLE_COLLECT;
		hm_int_insert(&hm, *k, (int)i);
		// CALLGRIND_TOGGLE_COLLECT;
		// CALLGRIND_STOP_INSTRUMENTATION;
		u8mem_delete(k);
	}

	clock_t t0 = clock();

	/* Main random workload: mix of insert/search/remove */
	size_t op = 0;
	for (; op < N_ops; ++op)
	{
		uint32_t r = xorshift32(&rng);
		double choice = (double)(r & 0xFFFF) / (double)0xFFFF;

		if (choice < insert_ratio)
		{
			/* Insert new key (value = op) */
			struct u8mem *k = mkkey_from_uint(keybuf, keybuf_len, r);
			// CALLGRIND_START_INSTRUMENTATION;
			// CALLGRIND_TOGGLE_COLLECT;
			int *p = hm_int_insert(&hm, *k, (int)op);
			// CALLGRIND_TOGGLE_COLLECT;
			// CALLGRIND_STOP_INSTRUMENTATION;

			if (p)
				*p = (int)op;

			u8mem_delete(k);
		}
		else
		{
			/* Lookup existing key */
			uint32_t q = xorshift32(&rng);
			struct u8mem *k = mkkey_from_uint(keybuf, keybuf_len, q);
			// CALLGRIND_START_INSTRUMENTATION;
			// CALLGRIND_TOGGLE_COLLECT;
			(void)hm_int_search(hm, *k);
			// CALLGRIND_TOGGLE_COLLECT;
			// CALLGRIND_STOP_INSTRUMENTATION;
			u8mem_delete(k);
		}

		/* Occasionally emulate entering and leaving a scope:
		   create short-lived symbols and then remove them. */
		if ((op & 0x3FFF) == 0)
		{
			size_t scope_sz = 32;
			for (size_t i = 0; i < scope_sz; ++i)
			{
				struct u8mem *k =
					mkkey_from_uint(keybuf, keybuf_len, (uint32_t)(r + i));
				// CALLGRIND_START_INSTRUMENTATION;
				// CALLGRIND_TOGGLE_COLLECT;
				hm_int_insert(&hm, *k, (int)i);
				// CALLGRIND_TOGGLE_COLLECT;
				// CALLGRIND_STOP_INSTRUMENTATION;
				u8mem_delete(k);
			}

			for (size_t i = 0; i < scope_sz; ++i)
			{
				struct u8mem *k =
					mkkey_from_uint(keybuf, keybuf_len, (uint32_t)(r + i));
				int tmp;
				// CALLGRIND_START_INSTRUMENTATION;
				// CALLGRIND_TOGGLE_COLLECT;
				hm_int_remove(hm, &tmp, *k);
				// CALLGRIND_TOGGLE_COLLECT;
				// CALLGRIND_STOP_INSTRUMENTATION;
				u8mem_delete(k);
			}
		}
	}

	clock_t t1 = clock();
	double seconds = (double)(t1 - t0) / CLOCKS_PER_SEC;
	printf("ops=%zu time=%.6fs ops/sec=%.0f\n", op, seconds, op / seconds);

	// CALLGRIND_START_INSTRUMENTATION;
	// CALLGRIND_TOGGLE_COLLECT;
	hm_int_delete(hm, NULL);
	// CALLGRIND_TOGGLE_COLLECT;
	// CALLGRIND_STOP_INSTRUMENTATION;
	return 0;
}
