/*!
 * Example usage (after building):
 * `./bench_hashmap --iterations=500000 --capacity=1000 --seed=1234 --insert-ratio=0.55`
 */

#define _GNU_SOURCE

#include <getopt.h>  // getopt_long
#include <limits.h>  // CHAR_BIT
#include <stdint.h>  // int32_t
#include <stdio.h>   // sprintf
#include <stdlib.h>  // strtoul, strtod
#include <time.h>    // clock

#include "HashMap.h"
#include "u8mem.h"

#define ERROR(message)                                                        \
	fprintf(stderr, "%s:%d " message "\n", __FILE__, __LINE__)

/* HASHMAP STATS */

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

/* BINARY FORMAT */

static char *bin(uintmax_t n, char outbuf[CHAR_BIT * sizeof(uintmax_t) + 1])
{
	char *p = outbuf;

	do
	{
		*p = '0' + (n & 1);
		p++;
		n >>= 1;
	} while (n);

	*p = '\0';
	return (outbuf);
}

static bool write_key(u8mem *const key, uintmax_t n)
{
	static char binbuf[CHAR_BIT * sizeof(uintmax_t) + 1];
	const long len = sprintf(
		(char *)key->buf, "sym_0b%.*s", (int)sizeof(binbuf), bin(n, binbuf)
	);

	if (len < 0)
		return (false);

	key->len = len;
	return (true);
}

/* QUEUE */

typedef struct Node
{
	struct Node *next;
	struct Node *prev;
	unsigned int data;
} Node;

typedef struct
{
	Node *head;
	Node *tail;
} Queue;

static Node *nd_new(const unsigned int data)
{
	Node *const nd = calloc(1, sizeof(*nd));

	if (nd)
		nd->data = data;

	return (nd);
}

static unsigned int nd_delete(Node *const nd)
{
	const unsigned int data = nd->data;

	*nd = (Node){0};
	free(nd);
	return (data);
}

static Node *q_push_tail(Queue *const restrict q, Node *const restrict nd)
{
	if (!nd)
		return (NULL);

	nd->next = NULL;
	nd->prev = q->tail;
	if (q->tail)
		q->tail->next = nd;

	q->tail = nd;
	if (!q->head)
		q->head = nd;

	return (nd);
}

static Node *q_push_head(Queue *const restrict q, Node *const restrict nd)
{
	if (!nd)
		return (NULL);

	nd->next = q->head;
	nd->prev = NULL;
	if (q->head)
		q->head->prev = nd;

	q->head = nd;
	if (!q->tail)
		q->tail = nd;

	return (nd);
}

static Node *q_pop_head(Queue *const q)
{
	if (!q->head)
		return (NULL);

	Node *const nd = q->head;

	if (q->tail == q->head)
		q->tail = NULL;

	q->head = q->head->next;
	return (nd);
}

static void q_clear(Queue *const q)
{
	while (q->head)
	{
		Node *const next = q->head->next;

		nd_delete(q->head);
		q->head = next;
	}

	*q = (Queue){0};
}

/*****************************************************************************/

int main(int argc, char **argv)
{
	size_t iterations = 250000;
	unsigned int seed = 42;
	/* fraction of operations that are inserts */
	double insert_ratio = 0.55;
	size_t initial_capacity = 4093;
	const char short_opts[] = "c:r:s:n:";
	const struct option long_opts[] = {
		{"capacity", required_argument, NULL, 'c'},
		{"iterations", required_argument, NULL, 'n'},
		{"insert-ratio", required_argument, NULL, 'r'},
		{"seed", required_argument, NULL, 's'},
		{0, 0, 0, 0}
	};

	for (int opt = getopt_long(argc, argv, short_opts, long_opts, NULL);
		 opt != -1;)
	{
		char *end = "nill";

		switch (opt)
		{
		case 'c':
			initial_capacity = strtoul(optarg, &end, 0);
			break;
		case 'n':
			iterations = strtoul(optarg, &end, 0);
			break;
		case 'r':
			insert_ratio = strtod(optarg, &end);
			break;
		case 's':
			seed = (unsigned int)strtoul(optarg, &end, 0);
			break;
		case '?':
			return (1);
		default:
			break;
		}

		if (*end)
		{
			fprintf(stderr, "Invalid argument: %s\n", optarg);
			return (1);
		}

		opt = getopt_long(argc, argv, short_opts, long_opts, NULL);
	}

	/* Create the map */
	struct HashMap_uint *restrict hm = hm_uint_new(initial_capacity);
	if (!hm)
	{
		ERROR("failed to create hashmap");
		return (1);
	}

	int status = 0;
	unsigned int rng = 0;
	unsigned char keybuf[128];
	u8mem mem = {.len = sizeof(keybuf), .buf = keybuf};
	Queue keys = {0};

	srand(seed);
	/* Warm-up: insert initial keys to populate the table a bit */
	for (size_t i = 0; i < initial_capacity / 2; ++i)
	{
		if (!write_key(&mem, rand()) || !hm_uint_insert(&hm, mem, i))
		{
			ERROR("failed to insert key into hash map");
			status = 1;
			goto cleanup;
		}
	}

	size_t op = 0, inserts = 0, search_hits = 0, scopes = 0, scope_inserts = 0;
	clock_t start = clock();

	/* Main random workload: mix of insert/search/remove */
	for (; op < iterations; ++op)
	{
		rng = rand();
		const double choice = (double)rng / (double)RAND_MAX;

		if (choice < insert_ratio) /* Insert new key (value = op) */
		{
			if (!write_key(&mem, rng) || !hm_uint_insert(&hm, mem, op))
			{
				ERROR("failed to insert key into hash map");
				status = 1;
				goto cleanup;
			}

			if (!q_push_tail(&keys, nd_new(rng)))
			{
				ERROR("failed to push key onto queue");
				status = 1;
				goto cleanup;
			}

			inserts++;
		}
		else /* Lookup existing key */
		{
			Node *const prev_key = q_pop_head(&keys);

			if (prev_key)
			{
				if (!write_key(&mem, nd_delete(prev_key)) ||
					!hm_uint_search(hm, mem))
				{
					ERROR("key search failed");
					status = 1;
					goto cleanup;
				}

				search_hits++;
			}
			else /* No existing keys to search for */
			{
				write_key(&mem, rng);
				search_hits += (bool)hm_uint_search(hm, mem);
			}
		}

		/* Occasionally emulate entering and leaving a scope:
		   create short-lived symbols and then remove them. */
		if ((rng & 0x7F) == 0)
		{
			unsigned scope_sz = rng & 31;

			if (scope_sz < 3)
				scope_sz += 3;

			scopes++;
			scope_inserts += scope_sz;
			for (unsigned i = 1; i <= scope_sz; ++i)
			{
				rng = rand();
				if (!q_push_head(&keys, nd_new(rng)) ||
					!write_key(&mem, rng) || !hm_uint_insert(&hm, mem, op + i))
				{
					ERROR("failed to insert key into hash map");
					status = 1;
					goto cleanup;
				}
			}

			for (unsigned i = 1; i <= scope_sz; ++i)
			{
				if (!write_key(&mem, nd_delete(q_pop_head(&keys))) ||
					!hm_uint_remove(hm, NULL, mem))
				{
					ERROR("failed to remove key from hash map");
					status = 1;
					goto cleanup;
				}
			}
		}
	}

	const double seconds = (double)(clock() - start) / CLOCKS_PER_SEC;
cleanup:
	if (status == 0)
	{
		printf("OK operations=%zu time=%.6fs ", op, seconds);
		printf("operations/sec=%.0f inserts=%zu ", op / seconds, inserts);
		printf("hits=%zu scopes=%zu ", search_hits, scopes);
		printf("scope_inserts=%zu ", scope_inserts);
		putchar('\n');
	}
	else
		printf("FAIL ");

	hm_stats_print(hm);
	hm_uint_delete(hm, NULL);
	q_clear(&keys);
	return (status);
}
