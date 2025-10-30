#ifndef HASHMAP_UNIQUE_SUFFIX
	#error "Missing definition for `HASHMAP_UNIQUE_SUFFIX`."
#endif /* HASHMAP_UNIQUE_SUFFIX */

#ifndef HASHMAP_DATATYPE
	#error "Missing definition for `HASHMAP_DATATYPE`."
#endif /* HASHMAP_DATATYPE */

#include <stddef.h> /* offsetof */
#include <stdio.h>  /* sprintf */
#include <string.h> /* memcmp */

#define COMMON_CALLBACKS_UNIQUE_SUFFIX HASHMAP_UNIQUE_SUFFIX
#define COMMON_CALLBACKS_DATATYPE HASHMAP_DATATYPE
#include "common_callback_types.h"

#include "xalloc/xalloc.h"

#if !defined FNV32A_HASH_FUNC

	#ifndef MURMURHASH3_x86_32_HASH_FUNC
		#define MURMURHASH3_x86_32_HASH_FUNC
	#endif /* MURMURHASH3_x86_32_HASH_FUNC */

	#include "MurmurHash3.c"

#elif defined FNV32A_HASH_FUNC
	#include "FNV-1a.c"
#endif /* !defined FNV32A_HASH_FUNC */

/* Helper macro functions. */

// #define POWER2_ROUNDUP_FUNC

#ifdef POWER2_ROUNDUP_FUNC
	#include "roundup.c"

	#define FOLD(hash, capacity) (hash & (capacity - 1))
#else
	#define FOLD(hash, capacity) (hash % capacity)
#endif /* POWER2_ROUNDUP_FUNC */

#define POS_TO_PTR(array, position)                                           \
	((position) > 0 ? &(array)[(position) - 1] : NULL)
#define PTR_TO_POS(array, pointer) ((pointer) ? (pointer) - (array) + 1 : 0)

/* Type specific name builders */

#define HM_CONCAT0(tok0, tok1) tok0##tok1
#define HM_CONCAT(tok0, tok1) HM_CONCAT0(tok0, tok1)

#define HASHMAP_STRUCT_TAG HM_CONCAT(HashMap_, HASHMAP_UNIQUE_SUFFIX)
#define BUCKET_STRUCT_TAG HM_CONCAT(Bucket_, HASHMAP_UNIQUE_SUFFIX)
#define CELLAR_STRUCT_TAG HM_CONCAT(Cellar_, HASHMAP_UNIQUE_SUFFIX)

#define HASHMAP_METHODNAME(name)                                              \
	HM_CONCAT(HM_CONCAT(hm_, HASHMAP_UNIQUE_SUFFIX), HM_CONCAT(_, name))
#define BUCKET_METHODNAME(name)                                               \
	HM_CONCAT(HM_CONCAT(bkt_, HASHMAP_UNIQUE_SUFFIX), HM_CONCAT(_, name))

/* Forward declarations. */

static bool BUCKET_METHODNAME(islive)(const BUCKET_STRUCT_TAG bkt);
static void BUCKET_METHODNAME(unlink)(
	HASHMAP_STRUCT_TAG *const restrict hm,
	BUCKET_STRUCT_TAG *const restrict bkt
) _nonnull;
static char *BUCKET_METHODNAME(tostr)(
	const BUCKET_STRUCT_TAG bucket,
	HM_CONCAT(stringify_data_, HASHMAP_UNIQUE_SUFFIX) * data_tostr
) _nonnull;

static HASHMAP_STRUCT_TAG *
	HASHMAP_METHODNAME(double_capacity)(HASHMAP_STRUCT_TAG *const hm) _nonnull;
static BUCKET_STRUCT_TAG *
	HASHMAP_METHODNAME(get_empty)(HASHMAP_STRUCT_TAG *const hm);
static bool HASHMAP_METHODNAME(isvalid)(const HASHMAP_STRUCT_TAG *const hm);
static BUCKET_STRUCT_TAG *HASHMAP_METHODNAME(place)(
	HASHMAP_STRUCT_TAG *const hm, BUCKET_STRUCT_TAG bucket
) _nonnull;
static BUCKET_STRUCT_TAG *HASHMAP_METHODNAME(search_key)(
	HASHMAP_STRUCT_TAG *const hm, const hash_ty hash, const u8mem key
);

/*!
 * @brief check if a `Bucket` is in use.
 *
 * @param bkt the Bucket to check.
 * @returns true if the bucket is in use, false otherwise.
 */
static bool BUCKET_METHODNAME(islive)(const BUCKET_STRUCT_TAG bkt)
{
	return (bkt.key != NULL);
}

/**
 * @brief allocate and initialise memory for a `HashMap`.
 *
 * @param capacity number of buckets the HashMap will contain.
 * @returns pointer to the hash map success, NULL on failure.
 */
HASHMAP_STRUCT_TAG *HASHMAP_METHODNAME(new)(len_ty capacity)
{
	if (capacity < 1)
		return (NULL);

#ifdef POWER2_ROUNDUP_FUNC
	capacity = power2_roundup(capacity);
#endif /* POWER2_ROUNDUP_FUNC */
#ifdef CELLAR_COALESCED_HASHING
	/* https://en.wikipedia.org/wiki/Coalesced_hashing#The_cellar */
	const len_ty cellar_capacity = (capacity * 14) / 86;
#else
	const len_ty cellar_capacity = 0;
#endif /* CELLAR_COALESCED_HASHING */
	const size_t cellar_size = sizeof(BUCKET_STRUCT_TAG) * cellar_capacity;
	const size_t size = sizeof(BUCKET_STRUCT_TAG) * capacity;

	/* overflow errors. */
	if (SIZE_MAX - cellar_size < size ||
		SIZE_MAX - (cellar_size + size) < sizeof(HASHMAP_STRUCT_TAG) ||
		(cellar_size + size) / sizeof(BUCKET_STRUCT_TAG) !=
			((size_t)cellar_capacity + capacity))
		return (NULL);

#ifdef EMPTY_BUCKET_STACK
	HASHMAP_STRUCT_TAG *const restrict table =
		xmalloc(sizeof(*table) + size + cellar_size);
#else
	HASHMAP_STRUCT_TAG *const restrict table =
		xcalloc(1, sizeof(*table) + size + cellar_size);
#endif /* EMPTY_BUCKET_STACK */

	if (!table)
		return (NULL);

	*table = (HASHMAP_STRUCT_TAG){.capacity = capacity};
#ifdef CELLAR_COALESCED_HASHING
	table->cellar = (CELLAR_STRUCT_TAG){.capacity = cellar_capacity};
#endif /* CELLAR_COALESCED_HASHING */
#ifdef EMPTY_BUCKET_STACK
	size_t i = table->capacity;

	/* Iterating from the end of the array ensures that the buckets */
	/* in the cellar (which is usually immediately after the main array) */
	/* are placed at the top of the stack. */
	#ifdef CELLAR_COALESCED_HASHING
	i += table->cellar.capacity;
	#endif /* CELLAR_COALESCED_HASHING */
	table->top_pos = i;
	/* top.prev_pos should be 0. */
	unsigned int prev_pos = 0;
	while (i > 0)
	{
		const unsigned int curr_pos = i;

		i--;
		table->arr[i] = (BUCKET_STRUCT_TAG){
			.next_pos = curr_pos - 1,
			.prev_pos = prev_pos,
		};

		prev_pos = curr_pos;
	}

#endif /* EMPTY_BUCKET_STACK */
	return (table);
}

/*!
 * @brief check if a `HashMap` is in a valid state.
 *
 * @param hm the HashMap to check.
 * @returns true if valid, false otherwise.
 */
static bool HASHMAP_METHODNAME(isvalid)(const HASHMAP_STRUCT_TAG *const hm)
{
	bool ok = hm->capacity > 0 && hm->used >= 0;

#ifdef CELLAR_COALESCED_HASHING
	ok = ok && hm->cellar.capacity >= 0 && hm->cellar.used >= 0;
#endif /* CELLAR_COALESCED_HASHING */
	return (ok);
}

/**
 * @brief free memory allocated to a HashMap.
 *
 * @param hm pointer to the HashMap to delete.
 * @param data_free pointer to a function that can free the data in the buckets.
 * @returns NULL always.
 */
void *HASHMAP_METHODNAME(delete)(
	HASHMAP_STRUCT_TAG *const restrict hm,
	HM_CONCAT(free_mem_, HASHMAP_UNIQUE_SUFFIX) * data_free
)
{
	if (!hm)
		return (NULL);

	if (HASHMAP_METHODNAME(isvalid)(hm) == false)
		goto free_hashmap;

	len_ty i = hm->capacity;

#ifdef CELLAR_COALESCED_HASHING
	i += hm->cellar.capacity;
#endif /* CELLAR_COALESCED_HASHING */
	while (i > 0)
	{
		i--;
		if (BUCKET_METHODNAME(islive)(hm->arr[i]) == false)
			continue;

		if (data_free)
			data_free(hm->arr[i].data);

		hm->arr[i].key = xfree(hm->arr[i].key);
		hm->arr[i] = (BUCKET_STRUCT_TAG){0};
	}

free_hashmap:
	*hm = (HASHMAP_STRUCT_TAG){0};
	xfree(hm);
	return (NULL);
}

/*!
 * @brief duplicate a `HashMap`.
 *
 * @param hm pointer to the HashMap to duplicate.
 * @param data_dup function that can duplicate the data in the HashMap.
 * @param data_free function that can delete the data in the HashMap, this
 * function must be provided if `data_dup` is provided.
 *
 * @returns pointer to the duplicate HashMap, NULL on error.
 */
HASHMAP_STRUCT_TAG *HASHMAP_METHODNAME(copy)(
	const HASHMAP_STRUCT_TAG *const restrict hm,
	HM_CONCAT(duplicate_, HASHMAP_UNIQUE_SUFFIX) data_dup,
	HM_CONCAT(free_mem_, HASHMAP_UNIQUE_SUFFIX) data_free
)
{
	if (!hm || HASHMAP_METHODNAME(isvalid)(hm) == false ||
		(data_dup && !data_free))
		return (NULL);

	HASHMAP_STRUCT_TAG *const restrict cpy =
		HASHMAP_METHODNAME(new)(hm->capacity);

	if (!cpy)
		return (NULL);

	len_ty i = hm->capacity;

#ifdef CELLAR_COALESCED_HASHING
	i += hm->capacity;
#endif /* CELLAR_COALESCED_HASHING */
	while (i > 0)
	{
		i--;
		cpy->arr[i] = hm->arr[i];
		if (BUCKET_METHODNAME(islive)(hm->arr[i]) == true)
		{
			cpy->arr[i].key =
				u8mem_new(hm->arr[i].key->buf, hm->arr[i].key->len);
			if (!cpy->arr[i].key)
				return (HASHMAP_METHODNAME(delete)(cpy, data_free));

			if (data_dup &&
				data_dup(&cpy->arr[i].data, hm->arr[i].data) == false)
				return (HASHMAP_METHODNAME(delete)(cpy, data_free));
		}
	}

	return (cpy);
}

/*!
 * @brief calculate the hash value of a given key.
 *
 * @param dest address to store the hash.
 * @param key block of memory to hash.
 * @returns true on success, false otherwise.
 */
bool HASHMAP_METHODNAME(hash)(hash_ty *const restrict dest, const u8mem key)
{
	if (!dest || !key.buf || key.len < 1)
		return (false);

#ifdef FNV32A_HASH_FUNC
	// FNV-1 and FNV-1a 32 bit non-zero initial basis.
	// FNV-0 uses 0 as the the initial value.
	*dest = 0x811c9dc5U;
	fnv32a_hash(key.buf, key.len, dest);
#elif defined MURMURHASH3_x86_32_HASH_FUNC
	*dest = 0;
	murmurhash3_x86_32(key.buf, key.len, dest);
#endif

	return (true);
}

/*!
 * @brief search for a `Bucket` with a particular hash and key.
 *
 * @param hm the HashMap to search.
 * @param hash the hash to search for.
 * @param key the key to search for.
 * @returns pointer to the bucket if found, NULL if not found.
 */
static BUCKET_STRUCT_TAG *HASHMAP_METHODNAME(search_key)(
	HASHMAP_STRUCT_TAG *const hm, const hash_ty hash, const u8mem key
)
{
	const unsigned int index = FOLD(hash, hm->capacity);
	BUCKET_STRUCT_TAG *restrict walk = &hm->arr[index];

	if (BUCKET_METHODNAME(islive)(*walk) == false)
		return (NULL);

	while (walk)
	{
		if (hash == walk->hash && u8mem_compare(key, *walk->key) == 0)
			return (walk);

		walk = POS_TO_PTR(hm->arr, walk->next_pos);
	}

	return (NULL);
}

/*!
 * @brief search for a `Bucket` with the same key as the given key.
 *
 * @param hm the HashMap to search.
 * @param key the key to search for.
 * @returns pointer to the data in the Bucket with the same key, NULL on
 * failure.
 */
HASHMAP_DATATYPE *
HASHMAP_METHODNAME(search)(HASHMAP_STRUCT_TAG *const hm, const u8mem key)
{
	if (HASHMAP_METHODNAME(isvalid)(hm) == false)
		return (NULL);

	hash_ty hash;

	if (!HASHMAP_METHODNAME(hash)(&hash, key))
		return (NULL);

	BUCKET_STRUCT_TAG *const restrict bucket =
		HASHMAP_METHODNAME(search_key)(hm, hash, key);

	if (!bucket)
		return (NULL);

	return (&bucket->data);
}

/*!
 * @brief grow the capacity of a `HashMap` to the given capacity.
 *
 * @param hm pointer to the HashMap.
 * @param capacity the new capacity.
 * @returns pointer to the expanded HashMap, NULL on failure.
 */
HASHMAP_STRUCT_TAG *HASHMAP_METHODNAME(grow)(
	HASHMAP_STRUCT_TAG *const restrict hm, const len_ty capacity
)
{
	if (!hm || capacity < 1 || HASHMAP_METHODNAME(isvalid)(hm) == false)
		return (NULL);

	if (capacity <= hm->capacity)
		return (hm);

#ifdef POWER2_ROUNDUP_FUNC
	HASHMAP_STRUCT_TAG *const restrict new_hm =
		HASHMAP_METHODNAME(new)(power2_roundup(capacity));
#else
	HASHMAP_STRUCT_TAG *const restrict new_hm =
		HASHMAP_METHODNAME(new)(capacity);
#endif /* POWER2_ROUNDUP_FUNC */

	if (!new_hm)
		return (NULL);

	len_ty i = hm->capacity;

#ifdef CELLAR_COALESCED_HASHING
	i += hm->cellar.capacity;
#endif /* CELLAR_COALESCED_HASHING */
	while (i > 0)
	{
		i--;
		if (BUCKET_METHODNAME(islive)(hm->arr[i]) == false)
			continue;

		/* slots should not run out. */
		HASHMAP_METHODNAME(place)(new_hm, hm->arr[i]);
		hm->arr[i] = (BUCKET_STRUCT_TAG){0};
	}

	HASHMAP_METHODNAME(delete)(hm, NULL);
	return (new_hm);
}

/*!
 * @brief double the capacity of a `HashMap` if needed.
 *
 * @param hm pointer to the HashMap to grow.
 * @returns pointer to the doubled HashMap, NULL on failure.
 */
static HASHMAP_STRUCT_TAG *
HASHMAP_METHODNAME(double_capacity)(HASHMAP_STRUCT_TAG *const restrict hm)
{
#ifdef CELLAR_COALESCED_HASHING
	if (hm->cellar.used < hm->cellar.capacity)
		return (hm);
#endif /* CELLAR_COALESCED_HASHING */

	if (hm->used <= HASHMAP_MAX_LOAD_FACTOR * hm->capacity)
		return (hm);

	HASHMAP_STRUCT_TAG *const restrict new_hm =
		HASHMAP_METHODNAME(grow)(hm, hm->capacity * 2);

	return (new_hm);
}

/*!
 * @brief unlink a bucket node from a linked list.
 *
 * @param hm pointer to the HashMap the bucket is in.
 * @param bkt pointer to the bucket.
 */
static void BUCKET_METHODNAME(unlink)(
	HASHMAP_STRUCT_TAG *const restrict hm,
	BUCKET_STRUCT_TAG *const restrict bkt
)
{
#ifdef EMPTY_BUCKET_STACK
	if (POS_TO_PTR(hm->arr, hm->top_pos) == bkt)
		hm->top_pos = bkt->next_pos;

#endif /* EMPTY_BUCKET_STACK */
	const unsigned int next_pos = bkt->next_pos;
	const unsigned int prev_pos = bkt->prev_pos;
	if (next_pos)
		POS_TO_PTR(hm->arr, next_pos)->prev_pos = prev_pos;

	if (prev_pos)
		POS_TO_PTR(hm->arr, prev_pos)->next_pos = next_pos;

	bkt->next_pos = 0;
	bkt->prev_pos = 0;
}

/*!
 * @brief search a `HashMap` for an empty `Bucket`.
 *
 * The Cellar if available will be searched first.
 *
 * @param hm the HashMap to search.
 * @returns pointer to an empty Bucket, NULL if none.
 */
static BUCKET_STRUCT_TAG *
HASHMAP_METHODNAME(get_empty)(HASHMAP_STRUCT_TAG *const restrict hm)
{
	BUCKET_STRUCT_TAG *restrict empty_bucket = NULL;
#ifdef EMPTY_BUCKET_STACK

	if (hm->top_pos)
	{
		empty_bucket = POS_TO_PTR(hm->arr, hm->top_pos);
		BUCKET_METHODNAME(unlink)(hm, empty_bucket);
	}
#else
	len_ty i = 0;

	#ifdef CELLAR_COALESCED_HASHING
	/* Check the cellar first if it has unused slots. */
	if (hm->cellar.used < hm->cellar.capacity)
		i += hm->cellar.capacity;
	#endif /* CELLAR_COALESCED_HASHING */

	if (hm->used < hm->capacity || i > 0)
		i += hm->capacity;

	while (i > 0)
	{
		i--;
		if (BUCKET_METHODNAME(islive)(hm->arr[i]) == false)
		{
			empty_bucket = &hm->arr[i];
			break;
		}
	}
#endif     /* EMPTY_BUCKET_STACK */

	*empty_bucket = (BUCKET_STRUCT_TAG){0};
	return (empty_bucket);
}

/*!
 * @brief insert a `Bucket` into a `HashMap`.
 *
 * @param hm pointer to the HashMap.
 * @returns pointer to the inserted Bucket, NULL if there are no more slots.
 */
static BUCKET_STRUCT_TAG *HASHMAP_METHODNAME(place)(
	HASHMAP_STRUCT_TAG *const restrict hm, BUCKET_STRUCT_TAG bucket
)
{
	const len_ty index = FOLD(bucket.hash, hm->capacity);

	bucket.next_pos = 0;
	bucket.prev_pos = 0;
	if (BUCKET_METHODNAME(islive)(hm->arr[index]) == false)
	{
#ifdef EMPTY_BUCKET_STACK
		BUCKET_METHODNAME(unlink)(hm, &hm->arr[index]);
#endif /* EMPTY_BUCKET_STACK */
		hm->arr[index] = bucket;
		hm->used++;
		return (&hm->arr[index]);
	}

	BUCKET_STRUCT_TAG *const restrict alt_bucket =
		HASHMAP_METHODNAME(get_empty)(hm);
	if (!alt_bucket)
		return (NULL);

	/* Insert the bucket at the end of the chain. */
	for (BUCKET_STRUCT_TAG *restrict walk = &hm->arr[index]; walk;)
	{
		if (!walk->next_pos)
		{
			walk->next_pos = PTR_TO_POS(hm->arr, alt_bucket);
			bucket.prev_pos = PTR_TO_POS(hm->arr, walk);
			break;
		}

		walk = POS_TO_PTR(hm->arr, walk->next_pos);
	}

	*alt_bucket = bucket;
#ifdef CELLAR_COALESCED_HASHING
	if (hm->cellar.capacity > 0 && alt_bucket >= &hm->arr[hm->capacity])
		hm->cellar.used++;
	else
#endif /* CELLAR_COALESCED_HASHING */
		hm->used++;

	return (alt_bucket);
}

/*!
 * @brief insert a key data pair into a HashMap.
 *
 * The capacity of the HashMap will be doubled if the load factor is high.
 *
 * @param hm address of the pointer to the HashMap to modify.
 * @param key the key to insert.
 * @param data the data to insert.
 * @returns pointer to the data inserted, NULL on failure.
 */
HASHMAP_DATATYPE *HASHMAP_METHODNAME(insert)(
	HASHMAP_STRUCT_TAG *restrict *const hm, const u8mem key,
	HASHMAP_DATATYPE data
)
{
	if (!hm || !*hm || HASHMAP_METHODNAME(isvalid)(*hm) == false || !key.buf ||
		key.len < 1)
		return (NULL);

	hash_ty hash;

	if (!HASHMAP_METHODNAME(hash)(&hash, key))
		return (NULL);

	HASHMAP_STRUCT_TAG *map = *hm;
	BUCKET_STRUCT_TAG *restrict bucket =
		HASHMAP_METHODNAME(search_key)(map, hash, key);

	if (bucket)
	{
		bucket->data = data;
		return (&bucket->data);
	}

	u8mem *const restrict key_dup = u8mem_new(key.buf, key.len);

	if (!key_dup)
		return (NULL);

	map = HASHMAP_METHODNAME(double_capacity)(map);
	if (!map)
		return (u8mem_delete(key_dup));

	bucket = HASHMAP_METHODNAME(place)(
		map, (BUCKET_STRUCT_TAG){.data = data, .hash = hash, .key = key_dup}
	);
	*hm = map;
	return (&bucket->data);
}

/*!
 * @brief remove the `Bucket` containing the given key.
 *
 * @param hm pointer to the HashMap to edit.
 * @param dest address to store the data in the Bucket.
 * @param key the key of the bucket to remove.
 * @returns true on success, false on failure.
 */
bool HASHMAP_METHODNAME(remove)(
	HASHMAP_STRUCT_TAG *restrict hm, HASHMAP_DATATYPE *const restrict dest,
	const u8mem key
)
{
	if (!hm || HASHMAP_METHODNAME(isvalid)(hm) == false || !key.buf ||
		key.len < 1)
		return (false);

	hash_ty hash;

	if (!HASHMAP_METHODNAME(hash)(&hash, key))
		return (false);

	BUCKET_STRUCT_TAG *restrict removed =
		HASHMAP_METHODNAME(search_key)(hm, hash, key);
	if (!removed)
		return (false);

	BUCKET_STRUCT_TAG *restrict walk = POS_TO_PTR(hm->arr, removed->next_pos);

	BUCKET_METHODNAME(unlink)(hm, removed);
	if (dest)
		*dest = removed->data;

	*removed = (BUCKET_STRUCT_TAG){.key = u8mem_delete(removed->key)};
	while (walk)
	{
		BUCKET_STRUCT_TAG *const restrict next =
			POS_TO_PTR(hm->arr, walk->next_pos);
		BUCKET_STRUCT_TAG *restrict new_spot =
			&hm->arr[FOLD(walk->hash, hm->capacity)];

		walk->prev_pos = 0;
		if (BUCKET_METHODNAME(islive)(*new_spot) == true)
		{ /* place bucket at end of the chain. */
			while (new_spot->next_pos)
				new_spot = POS_TO_PTR(hm->arr, new_spot->next_pos);

			new_spot->next_pos = PTR_TO_POS(hm->arr, walk);
			walk->prev_pos = PTR_TO_POS(hm->arr, new_spot);
			new_spot = walk;
		}
		else
		{ /* place bucket at the start of chain and update the hole's position. */
			*new_spot = *walk;
			removed = walk;
			*removed = (BUCKET_STRUCT_TAG){0};
		}

		new_spot->next_pos = 0;
		walk = next;
	}

#ifdef EMPTY_BUCKET_STACK
	removed->next_pos = hm->top_pos;
	hm->top_pos = PTR_TO_POS(hm->arr, removed);
#endif /* EMPTY_BUCKET_STACK */
#ifdef CELLAR_COALESCED_HASHING
	if (hm->cellar.capacity > 0 && removed >= &hm->arr[hm->capacity])
		hm->cellar.used--;
	else
#endif /* CELLAR_COALESCED_HASHING */
		hm->used--;

	return (true);
}

/*!
 * @brief write the string representation of a `Bucket` into a buffer.
 *
 * @param bucket the Bucket.
 * @param buf the char buffer.
 * @param i cursor into the buffer.
 * @param data_tostr pointer to a function that can stringify the data in
 * the Bucket.
 * @returns pointer to the buffer, NULL on error.
 */
static char *BUCKET_METHODNAME(tostr)(
	const BUCKET_STRUCT_TAG bucket,
	HM_CONCAT(stringify_data_, HASHMAP_UNIQUE_SUFFIX) * data_tostr
)
{
	char *const restrict key_str = u8mem_tostr(*bucket.key);
	char *const restrict data_str = data_tostr(bucket.data);
	char *restrict bucket_str = NULL;

	if (!key_str || !data_str)
		goto cleanup;

	const long int len = snprintf(NULL, 0, "%s: %s", key_str, data_str) + 1;

	if (len < 1)
		goto cleanup;

	bucket_str = xmalloc(len);
	if (!bucket_str)
		goto cleanup;

	if (snprintf(bucket_str, len, "%s: %s", key_str, data_str) < 0)
		bucket_str = xfree(bucket_str);

cleanup:
	xfree(key_str);
	xfree(data_str);
	return (bucket_str);
}

/*!
 * @brief return the string representation of a `HashMap`.
 *
 * @param hm the HashMap to stringify.
 * @param data_tostr pointer to a function that can stringify the data in
 * the Buckets.
 * @returns pointer to the stringified data, NULL on failure.
 */
char *HASHMAP_METHODNAME(tostr)(
	const HASHMAP_STRUCT_TAG *const restrict hm,
	HM_CONCAT(stringify_data_, HASHMAP_UNIQUE_SUFFIX) * data_tostr
)
{
	if (HASHMAP_METHODNAME(isvalid)(hm) == false || !data_tostr)
		return (NULL);

	char *restrict hm_str = xcalloc(3, sizeof(*hm_str));

	if (!hm_str)
		return (NULL);

	hm_str[0] = '{';
	size_t s_len = 1;
#ifdef CELLAR_COALESCED_HASHING
	const len_ty end = hm->capacity + hm->cellar.capacity;
#else
	const len_ty end = hm->capacity;
#endif /* CELLAR_COALESCED_HASHING */

	for (len_ty i = 0; hm_str && i < end; i++)
	{
		if (BUCKET_METHODNAME(islive)(hm->arr[i]) == false)
			continue;

		char *const restrict bucket_str =
			BUCKET_METHODNAME(tostr)(hm->arr[i], data_tostr);

		if (!bucket_str)
			return (xfree(hm_str));

		size_t b_len = strlen(bucket_str) + 2;

		if (i < end - 1)
			b_len++;

		hm_str = xrealloc_free_on_fail(hm_str, s_len + b_len);
		if (hm_str)
		{
			if (i < end - 1)
				sprintf(hm_str + s_len, "%s, ", bucket_str);
			else
				sprintf(hm_str + s_len, "%s}", bucket_str);

			s_len += b_len;
		}

		xfree(bucket_str);
	}

	return (hm_str);
}

#undef FOLD

#undef HASHMAP_UNIQUE_SUFFIX
#undef HASHMAP_DATATYPE

#undef HM_CONCAT0
#undef HM_CONCAT

#undef HASHMAP_STRUCT_TAG
#undef BUCKET_STRUCT_TAG
#undef CELLAR_STRUCT_TAG

#undef HASHMAP_METHODNAME
#undef BUCKET_METHODNAME
