#define _POSIX_C_SOURCE 199309L /* clock_gettime */

#include <stdio.h>  /* fopen */
#include <stdlib.h> /* qsort */
#include <string.h> /* memset */
#include <time.h>   /* clock_gettime */

#include "HashMap.h"
#include "xalloc.h"

#define NSEC_IN_SEC (1000 * 1000 * 1000)

struct hashmap_stats
{
	unsigned int chains;
	unsigned int longest_chain_len;
	double avg_chain_len;
};

static unsigned char buffer[10 * 1000 * 1000];

static struct timespec timespec_add(struct timespec a, const struct timespec b)
{
	a.tv_nsec += b.tv_nsec;
	a.tv_sec += b.tv_sec + a.tv_nsec / NSEC_IN_SEC;
	a.tv_nsec %= NSEC_IN_SEC;
	return (a);
}

static struct timespec timespec_sub(struct timespec a, const struct timespec b)
{
	a.tv_sec -= b.tv_sec;
	a.tv_nsec -= b.tv_nsec;
	if (a.tv_nsec < 0)
	{
		a.tv_nsec += NSEC_IN_SEC;
		a.tv_sec--;
	}

	return (a);
}

static struct hashmap_stats hmstats_get(const HashMap_int *const map)
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
		const Bucket_int *bkt = &map->arr[i];

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

static void hmstats_print(const HashMap_int *const map)
{
	const struct hashmap_stats stats = hmstats_get(map);
#ifdef CELLAR_COALESCED_HASHING
	char strbuf[12] = {0};

	sprintf(strbuf, "%" PRI_len "+%" PRI_len, map->used, map->cellar.used);
	printf("%8s/", strbuf);
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

static void aggregate_timings(
	struct timespec *const restrict dest, const size_t block_size,
	const struct timespec *const restrict timings, const size_t len
)
{
	for (size_t i = 0; i < len; i++)
	{
		dest[i / block_size] = timespec_add(dest[i / block_size], timings[i]);
	}
}

static void
print_timings(const struct timespec *const restrict timings, const size_t len)
{
	for (unsigned int i = 0; i < len; i++)
	{
		printf("%.2u%%-%.2u%%: ", i * 5, i * 5 + 5);
		printf("%ld", timings[i].tv_sec);
		printf(".%.9lds", timings[i].tv_nsec);
		if (i < len - 1)
			printf(", ");
	}

	putchar('\n');
}

static bool profile_insertions(
	const len_ty initial_cap, const size_t max_inserts, const size_t key_size
)
{
	if (max_inserts * key_size > sizeof(buffer))
	{
		fprintf(stderr, "ERROR: " __FILE__ ":%d: ", __LINE__);
		fprintf(stderr, "not enough data to satisfy key inserts.\n");
		return (false);
	}

	size_t i = 0, expansions = 0;
	struct timespec total_time = {0};
	HashMap_int *restrict map = hm_int_new(initial_cap);
	bool success = true;
	struct timespec *restrict timings =
		xmalloc(sizeof(*timings) * max_inserts);

	if (!map || !timings)
	{
		success = false;
		goto cleanup;
	}

	for (; i < max_inserts; i++)
	{
		const u8mem key = {.len = key_size, .buf = &buffer[i * key_size]};
		const len_ty prev_cap = map->capacity;
		struct timespec start, end;

		if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start) != 0)
		{
			perror("could not get time");
			success = false;
			goto cleanup;
		}

		const int *const data = hm_int_insert(&map, key, i);

		if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end) != 0)
		{
			perror("could not get time");
			success = false;
			goto cleanup;
		}

		if (!data)
		{
			fprintf(stderr, "ERROR: " __FILE__ ":%d: ", __LINE__);
			fprintf(stderr, "could not insert key.\n");
			success = false;
			goto cleanup;
		}

		if (map->capacity > prev_cap)
			expansions++;

		timings[i] = timespec_sub(end, start);
		total_time = timespec_add(total_time, timings[i]);
	}

	printf("initial_capacity: 0/%-4" PRI_len ", ", initial_cap);
	printf("key_size: %4" PRI_len ", ", key_size);
	hmstats_print(map);
	printf("insertions: %4zu", i);
	printf(", expansions: %2zu", expansions);
	printf(", time: %ld", total_time.tv_sec);
	printf(".%.9lds\n", total_time.tv_nsec);
	const size_t percent5 =
		(map->capacity * 5) / 100 + (bool)((map->capacity * 5) % 100);
	struct timespec intervals[20] = {0};

	/* These are the timings for the insertion of a number of keys equal to */
	/* 5% of the HashMap capacity till the maximum number of inserts given. */
	printf("5%% capacity inserts time: ");
	aggregate_timings(intervals, percent5, timings, i);
	print_timings(intervals, 20);
cleanup:
	xfree(timings);
	hm_int_delete(map, NULL);
	return (success);
}

static bool profile_searches(
	HashMap_int *const restrict map, unsigned char *const restrict keys,
	const size_t key_count, const size_t key_size
)
{
	struct timespec total_time = {0};
	size_t hits = 0, i = 0;

	for (; i < key_count; i++)
	{
		const u8mem key = {.len = key_size, .buf = &keys[i * key_size]};
		struct timespec start, end;

		if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start) != 0)
		{
			perror("could not get time");
			return (false);
		}

		const int *const restrict d = hm_int_search(map, key);

		if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end) != 0)
		{
			perror("could not get time");
			return (false);
		}

		if (d)
			hits++;

		total_time = timespec_add(total_time, timespec_sub(end, start));
	}

	printf("hits: %4zu/%-4zu", hits, i);
	printf(", time: %ld", total_time.tv_sec);
	printf(".%.9lds", total_time.tv_nsec);
	return (true);
}

static bool profile_removes(
	HashMap_int *const restrict map, unsigned char *const restrict keys,
	const size_t key_count, const size_t key_size
)
{
	struct timespec total_time = {0};
	size_t removes = 0, i = 0;
	struct timespec *const restrict timings =
		xmalloc(sizeof(*timings) * key_count);

	printf("key_size: %4" PRI_len ", ", key_size);
	hmstats_print(map);
	for (; i < key_count; i++)
	{
		const u8mem key = {.len = key_size, .buf = &keys[i * key_size]};
		struct timespec start, end;

		if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start) != 0)
		{
			perror("could not get time");
			return (false);
		}

		removes += hm_int_remove(map, NULL, key);
		if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end) != 0)
		{
			perror("could not get time");
			return (false);
		}

		timings[i] = timespec_sub(end, start);
		total_time = timespec_add(total_time, timings[i]);
	}

	printf("removes: %4zu", removes);
	printf(", time: %ld", total_time.tv_sec);
	printf(".%.9lds\n", total_time.tv_nsec);
	const size_t percent5 =
		(map->capacity * 5) / 100 + (bool)((map->capacity * 5) % 100);
	struct timespec intervals[20] = {0};

	/* These are the timings for the removal of a number of keys equal to */
	/* 5% of HashMap capacity  from the HashMap till empty. */
	printf("5%% capacity inserts time: ");
	aggregate_timings(intervals, percent5, timings, i);
	print_timings(intervals, 20);
	xfree(timings);
	return (true);
}

static bool profile_general(
	HashMap_int *restrict *const restrict table, const len_ty key_size
)
{
	HashMap_int *restrict map = *table;
	const size_t cap = map->capacity;
	const size_t percent5 = (cap * 5) / 100 + (bool)((cap * 5) % 100);

	printf("------------------------- searches -------------------------\n");
	for (unsigned int i = 0; i < cap * HASHMAP_MAX_LOAD_FACTOR;)
	{
		unsigned int j = 0;
		for (; j < percent5 && i + j < cap * HASHMAP_MAX_LOAD_FACTOR; j++)
		{
			const u8mem key = {
				.len = key_size, .buf = &buffer[(i + j) * key_size]
			};
			const int *const restrict d = hm_int_insert(&map, key, i + j);

			if (!d)
			{
				fprintf(stderr, "ERROR: " __FILE__ ":%d: ", __LINE__);
				fprintf(stderr, "could not insert key.\n");
				*table = map;
				goto error_cleanup;
			}
		}

		i += j;
		printf("key_size: %4" PRI_len ", ", key_size);
		hmstats_print(map);
		/* All keys present. */
		if (!profile_searches(map, buffer, i, key_size))
			goto error_cleanup;

		printf(", ");
		/* Half the keys present. */
		if (!profile_searches(map, &buffer[i * key_size / 2], i, key_size))
			goto error_cleanup;

		printf(", ");
		/* None of the keys present. */
		if (!profile_searches(map, &buffer[i * key_size], i, key_size))
			goto error_cleanup;

		printf("\n\n");
	}

	printf("------------------------- removes -------------------------\n");
	profile_removes(map, buffer, map->used, key_size);

	*table = map;
	return (true);
error_cleanup:
	*table = map;
	return (false);
}

/*!
 * @brief run profiling tests on HashMap methods.
 *
 * @returns 0 on success, 1 on error.
 */
int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "usage: %s data-file\n", argv[0]);
		return (1);
	}

	FILE *const restrict data_file = fopen(argv[1], "r");

	if (!data_file)
	{
		perror("could not open file");
		return (1);
	}

	const size_t bytes_read = fread(buffer, 1, sizeof(buffer), data_file);

	fclose(data_file);
	if (bytes_read < sizeof(buffer))
	{
		perror("could not read enough data from file");
		return (1);
	}

	const len_ty key_sizes[] = {5, 16, 50, 100, 251, 379, 610};

	for (unsigned int i = 0; i < sizeof(key_sizes) / sizeof(*key_sizes); i++)
	{

		printf(
			"------------------------- insertions -------------------------\n"
		);
		for (len_ty cap = 8; cap <= 8192; cap *= 2)
		{
			if (!profile_insertions(
					cap - 1, (cap - 1) * HASHMAP_MAX_LOAD_FACTOR, key_sizes[i]
				))
				return (1);

			putchar('\n');
		}

		/* Using prime number for initial capacity. */
		HashMap_int *restrict map = hm_int_new(8191);

		if (!map)
			return (1);

		const bool success = profile_general(&map, key_sizes[i]);

		hm_int_delete(map, NULL);
		if (!success)
			return (1);

		putchar('\n');
	}

	return (0);
}
