/*
 * Example usage (after building):
 *   ./bench_hashmap --ops 500000 --seed 1234 --ratio 0.7 --scopes 1000 *
 */

// snprintf
#define _ISOC99_SOURCE

#include <getopt.h>
#include <limits.h>  // CHAR_BIT
#include <stdint.h>  // int32_t
#include <stdio.h>   // snprintf
#include <stdlib.h>  // strtoul, strtod
#include <time.h>    // clock
// #include <valgrind/callgrind.h>

#include "HashMap.h"
#include "u8mem.h"

struct hashmap_stats
{
	unsigned int chains;
	unsigned int longest_chain_len;
	double avg_chain_len;
};

static struct hashmap_stats hm_stats_get(const HashMap_uint *const map)
{
	struct hashmap_stats stats = {0};
	size_t total_chain_len = 0;
	len_ty i = map->capacity;

#ifdef CELLAR_COALESCED_HASHING
	i += map->cellar.capacity;
#endif /* CELLAR_COALESCED_HASHING */
	while (i > 0)
	{
		i--;
		const Bucket_uint *bkt = &map->arr[i];

		if (bkt->key && !bkt->prev_pos)
		{
			stats.chains++;
			unsigned int len = 1;

			while (bkt->next_pos)
			{
				len++;
				bkt = &map->arr[bkt->next_pos - 1];
			}

			if (len > stats.longest_chain_len)
				stats.longest_chain_len = len;

			total_chain_len += len;
		}
	}

	stats.avg_chain_len = total_chain_len / (double)stats.chains;
	return (stats);
}

static void hm_stats_print(const HashMap_uint *const map)
{
	const struct hashmap_stats stats = hm_stats_get(map);
#ifdef CELLAR_COALESCED_HASHING
	char strbuf[sizeof(map->capacity) * CHAR_BIT * 2] = {0};

	sprintf(strbuf, "%" PRI_len "+%" PRI_len, map->used, map->cellar.used);
	printf("capacity: %8s/", strbuf);
	sprintf(
		strbuf, "%" PRI_len "+%" PRI_len, map->capacity, map->cellar.capacity
	);
	printf("%-8s", strbuf);
#else

	printf("capacity: %4" PRI_len "/%-4" PRI_len, map->used, map->capacity);
#endif /* CELLAR_COALESCED_HASHING */
	printf(", chains: %3u", stats.chains);
	printf(", average_chain_length: %.2f", stats.avg_chain_len);
	printf(", longest_chain_length: %u\n", stats.longest_chain_len);
}

int main(int argc, char **argv)
{
	size_t max_ops = 500000;
	uint32_t seed = 42;
	/* fraction of operations that are inserts */
	double insert_ratio = 0.55;
	size_t initial_capacity = 1021;

	for (int opt = getopt(argc, argv, "n:s:r:c:"); opt != -1;)
	{
		char *end = "";

		switch (opt)
		{
		case 'n':
			max_ops = (size_t)strtoul(optarg, &end, 0);
			break;
		case 's':
			seed = (uint32_t)strtoul(optarg, &end, 0);
			break;
		case 'r':
			insert_ratio = strtod(optarg, &end);
			break;
		case 'c':
			initial_capacity = (size_t)strtoul(optarg, &end, 0);
			break;
		default:
			break;
		}

		if (*end)
		{
			fprintf(stderr, "Invalid argument: %s\n", optarg);
			return (1);
		}

		opt = getopt(argc, argv, "n:s:r:c:");
	}

	/* Create the map */
	struct HashMap_uint *restrict hm = hm_uint_new(initial_capacity);
	if (!hm)
	{
		fprintf(stderr, "failed to create hashmap\n");
		return 2;
	}

	int status = 0;
	uint32_t rng = 0;
	unsigned char keybuf[64];
	u8mem mem = {.len = sizeof(keybuf), .buf = keybuf};

	srand(seed);
	/* Warm-up: insert initial keys to populate the table a bit */
	for (size_t i = 0; i < initial_capacity / 2; ++i)
	{
		rng = rand();
		mem.len = snprintf((char *)mem.buf, sizeof(keybuf), "sym_%08x", rng);
		// CALLGRIND_START_INSTRUMENTATION;
		// CALLGRIND_TOGGLE_COLLECT;
		unsigned int *const p = hm_uint_insert(&hm, mem, i);
		// CALLGRIND_TOGGLE_COLLECT;
		// CALLGRIND_STOP_INSTRUMENTATION;
		if (!p)
		{
			status = 1;
			goto cleanup;
		}
	}

	size_t op = 0;
	uint32_t rng_prev = rng;
	clock_t start = clock();

	/* Main random workload: mix of insert/search/remove */
	for (; op < max_ops; ++op)
	{
		rng = rand();
		const double choice = (double)rng / (double)RAND_MAX;

		if (choice < insert_ratio)
		{
			/* Insert new key (value = op) */
			mem.len =
				snprintf((char *)mem.buf, sizeof(keybuf), "sym_%08x", rng);
			// CALLGRIND_START_INSTRUMENTATION;
			// CALLGRIND_TOGGLE_COLLECT;
			unsigned int *const p = hm_uint_insert(&hm, mem, op);
			// CALLGRIND_TOGGLE_COLLECT;
			// CALLGRIND_STOP_INSTRUMENTATION;
			if (!p)
			{
				status = 1;
				goto cleanup;
			}
		}
		else
		{
			/* Lookup existing key */
			mem.len = snprintf(
				(char *)mem.buf, sizeof(keybuf), "sym_%08x", rng_prev
			);
			rng_prev = rng;
			// CALLGRIND_START_INSTRUMENTATION;
			// CALLGRIND_TOGGLE_COLLECT;
			(void)hm_uint_search(hm, mem);
			// CALLGRIND_TOGGLE_COLLECT;
			// CALLGRIND_STOP_INSTRUMENTATION;
		}

		/* Occasionally emulate entering and leaving a scope:
		   create short-lived symbols and then remove them. */
		if ((rng & 0xFF) == 0)
		{
			const unsigned scope_sz = (rng & 63) + 1;

			for (unsigned i = 1; i <= scope_sz; ++i)
			{
				mem.len = snprintf(
					(char *)mem.buf, sizeof(keybuf), "sym_%08x", rng + i
				);
				// CALLGRIND_START_INSTRUMENTATION;
				// CALLGRIND_TOGGLE_COLLECT;
				unsigned int *const p = hm_uint_insert(&hm, mem, i);
				// CALLGRIND_TOGGLE_COLLECT;
				// CALLGRIND_STOP_INSTRUMENTATION;
				if (!p)
				{
					status = 1;
					goto cleanup;
				}
			}

			for (unsigned i = 1; i <= scope_sz; ++i)
			{
				mem.len = snprintf(
					(char *)mem.buf, sizeof(keybuf), "sym_%08x", rng + i
				);
				// CALLGRIND_START_INSTRUMENTATION;
				// CALLGRIND_TOGGLE_COLLECT;
				hm_uint_remove(hm, NULL, mem);
				// CALLGRIND_TOGGLE_COLLECT;
				// CALLGRIND_STOP_INSTRUMENTATION;
			}
		}
	}

	const double seconds = (double)(clock() - start) / CLOCKS_PER_SEC;
cleanup:
	if (status == 0)
	{
		printf("OK ops=%zu time=%.6fs ", op, seconds);
		printf("ops/sec=%.0f\n", op / seconds);
	}
	else
		printf("FAIL ");

	hm_stats_print(hm);
	// CALLGRIND_START_INSTRUMENTATION;
	// CALLGRIND_TOGGLE_COLLECT;
	hm_uint_delete(hm, NULL);
	// CALLGRIND_TOGGLE_COLLECT;
	// CALLGRIND_STOP_INSTRUMENTATION;
	return (status);
}
