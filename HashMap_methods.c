#ifndef HASHMAP_UNIQUE_SUFFIX
	#error "Missing definition for `HASHMAP_UNIQUE_SUFFIX`."
#endif /* HASHMAP_UNIQUE_SUFFIX */

#ifndef HASHMAP_DATATYPE
	#error "Missing definition for `HASHMAP_DATATYPE`."
#endif /* HASHMAP_DATATYPE */

#include <assert.h>
#include <limits.h>  /* SIZE_MAX */
#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */
#include <stdio.h>   /* sprintf */
#include <string.h>  /* strlen */

#include "compiler_attributes_macros.h"
#include "xalloc/xalloc.h"

#define COMMON_CALLBACKS_UNIQUE_SUFFIX HASHMAP_UNIQUE_SUFFIX
#define COMMON_CALLBACKS_DATATYPE HASHMAP_DATATYPE
#include "common_generic_callback_types.h"

#if !defined FNV32A_HASH_FUNC
	#ifndef MURMURHASH3_x86_32_HASH_FUNC
		#define MURMURHASH3_x86_32_HASH_FUNC
	#endif /* MURMURHASH3_x86_32_HASH_FUNC */

	#include "MurmurHash3.c"
#elif defined FNV32A_HASH_FUNC
	#include "FNV-1a.c"
#endif /* !defined FNV32A_HASH_FUNC */

/************************* GENERICS BUILDER MACROS ***************************/

#define HM_CONCAT0(tok0, tok1) tok0##tok1
#define HM_CONCAT(tok0, tok1) HM_CONCAT0(tok0, tok1)

#define HASHMAP_TAG HM_CONCAT(HashMap_, HASHMAP_UNIQUE_SUFFIX)
#define BUCKET_TAG HM_CONCAT(Bucket_, HASHMAP_UNIQUE_SUFFIX)
#define CELLAR_TAG HM_CONCAT(Cellar_, HASHMAP_UNIQUE_SUFFIX)

/* clang-format off */
#define HASHMAP_METHOD(name) HM_CONCAT(HM_CONCAT(hm_, HASHMAP_UNIQUE_SUFFIX), HM_CONCAT(_, name))
#define BUCKET_METHOD(name) HM_CONCAT(HM_CONCAT(bkt_, HASHMAP_UNIQUE_SUFFIX), HM_CONCAT(_, name))
/* clang-format on */

/***************************** MACRO FUNCTIONS *******************************/

// #define POWER2_ROUNDUP_FUNC

#ifdef POWER2_ROUNDUP_FUNC
	#include "roundup.c"

	#define FOLD(hash, capacity) (hash & (capacity - 1))
#else
	#define FOLD(hash, capacity) (hash % capacity)
#endif /* POWER2_ROUNDUP_FUNC */

#define POS_TO_PTR(array, position) ((position) > 0 ? (array) + (position) - 1 : NULL)
#define PTR_TO_POS(array, pointer) ((pointer) ? (pointer) - (array) + 1 : 0)

/* clang-format off */
#define ASSERT_MAX_POSITION(position, max) assert((size_t)(position) <= (size_t)(max) && "position out of bounds")
#define ASSERT_POINTER_BOUNDS(pointer, min, max) assert((pointer) == NULL || ((pointer) >= (min) && (pointer) <= (max) && "pointer out of bounds"))
/* clang-format on */

/***************************** STATIC FUNCTIONS ******************************/

static bool HASHMAP_METHOD(isvalid)(const HASHMAP_TAG *const hm);
static bool BUCKET_METHOD(islive)(const BUCKET_TAG bucket);
static BUCKET_TAG *HASHMAP_METHOD(search_list)(
	HASHMAP_TAG *const hm, const hash_ty hash, const u8mem key
) _nonnull;
static BUCKET_TAG *HASHMAP_METHOD(get_empty)(HASHMAP_TAG *const hm) _nonnull;
static void BUCKET_METHOD(unlink)(
	HASHMAP_TAG *const restrict hm, BUCKET_TAG *const bucket
) _nonnull;
static void BUCKET_METHOD(insert_after)(
	HASHMAP_TAG *const restrict hm, BUCKET_TAG *const restrict bucket,
	BUCKET_TAG *const restrict here
) _nonnull_pos(1);
void BUCKET_METHOD(insert_before)(
	HASHMAP_TAG *const restrict hm, BUCKET_TAG *const restrict bucket,
	BUCKET_TAG *const restrict here
) _nonnull_pos(1);
static BUCKET_TAG *BUCKET_METHOD(list_tail)(
	HASHMAP_TAG *const restrict hm, BUCKET_TAG *const head
) _nonnull;
static BUCKET_TAG *
	HASHMAP_METHOD(place)(HASHMAP_TAG *const hm, BUCKET_TAG bucket) _nonnull;
static HASHMAP_TAG *
	HASHMAP_METHOD(double_capacity)(HASHMAP_TAG *const hm) _nonnull;

/*!
 * @brief check if a `Bucket` is in use.
 *
 * @param bucket the Bucket to check.
 * @returns true if the bucket is in use, false otherwise.
 */
static bool BUCKET_METHOD(islive)(const BUCKET_TAG bucket)
{
	return (bucket.key != NULL);
}

/*!
 * @brief check if a `HashMap` is in a valid state.
 *
 * @param hm the HashMap to check.
 * @returns true if valid, false otherwise.
 */
static bool HASHMAP_METHOD(isvalid)(const HASHMAP_TAG *const hm)
{
	bool ok = hm && hm->capacity > 0;

	ok = ok && hm->used >= 0 && hm->used <= hm->capacity;
#ifdef CELLAR_COALESCED_HASHING
	ok = ok && hm->cellar.capacity >= 0;
	ok = ok && hm->cellar.used >= 0 && hm->cellar.used <= hm->cellar.capacity;
#endif /* CELLAR_COALESCED_HASHING */
#ifdef EMPTY_BUCKET_STACK
	#ifdef CELLAR_COALESCED_HASHING
	ok = ok && hm->top_pos <= ((size_t)hm->capacity + hm->cellar.capacity);
	ok = ok && hm->bottom_pos <= ((size_t)hm->capacity + hm->cellar.capacity);
	#else
	ok = ok && hm->top_pos <= (size_t)hm->capacity;
	ok = ok && hm->bottom_pos <= (size_t)hm->capacity;
	#endif /* CELLAR_COALESCED_HASHING */
#endif     /* EMPTY_BUCKET_STACK */

	return (ok);
}

/*!
 * @brief search for the matching key in a linked list of `Bucket`s.
 *
 * @param hm non-null pointer to the `HashMap` containing the Buckets.
 * @param hash hash value of the Bucket to look for.
 * @param key the key to search.
 * @returns pointer to the Bucket matching the given key, NULL if not found.
 */
static BUCKET_TAG *HASHMAP_METHOD(search_list)(
	HASHMAP_TAG *const hm, const hash_ty hash, const u8mem key
)
{
	BUCKET_TAG *walk = &hm->arr[FOLD(hash, hm->capacity)];

	if (BUCKET_METHOD(islive)(*walk) == false)
		return (NULL);

	while (walk)
	{
		if (walk->hash == hash && u8mem_compare(*walk->key, key) == 0)
			return (walk);

		walk = POS_TO_PTR(hm->arr, walk->next_pos);
#ifdef CELLAR_COALESCED_HASHING /* clang-format off */
		ASSERT_POINTER_BOUNDS(	walk, hm->arr, hm->arr + hm->capacity + hm->cellar.capacity - 1	);
#else  /* clang-format on */
		ASSERT_POINTER_BOUNDS(walk, hm->arr, hm->arr + hm->capacity - 1);
#endif /* CELLAR_COALESCED_HASHING */
	}

	return (NULL);
}

/*!
 * @brief unlink a bucket node from a linked list.
 *
 * @param hm non-null pointer to the HashMap the bucket is in.
 * @param bucket non-null pointer to the bucket.
 */
static void
BUCKET_METHOD(unlink)(HASHMAP_TAG *const restrict hm, BUCKET_TAG *const bucket)
{
#ifdef EMPTY_BUCKET_STACK
	BUCKET_TAG *const top = POS_TO_PTR(hm->arr, hm->top_pos);
	BUCKET_TAG *const bottom = POS_TO_PTR(hm->arr, hm->bottom_pos);

	#ifdef CELLAR_COALESCED_HASHING /* clang-format off */
	ASSERT_POINTER_BOUNDS(top, hm->arr, hm->arr + hm->capacity + hm->cellar.capacity - 1);
	ASSERT_POINTER_BOUNDS(bottom, hm->arr, hm->arr + hm->capacity + hm->cellar.capacity - 1);
	#else  /* clang-format on */
	ASSERT_POINTER_BOUNDS(top, hm->arr, hm->arr + hm->capacity - 1);
	ASSERT_POINTER_BOUNDS(bottom, hm->arr, hm->arr + hm->capacity - 1);
	#endif /* CELLAR_COALESCED_HASHING */
	if (top == bucket)
		hm->top_pos = bucket->next_pos;

	if (bottom == bucket)
		hm->bottom_pos = bucket->prev_pos;

#endif /* EMPTY_BUCKET_STACK */
	BUCKET_TAG *const next = POS_TO_PTR(hm->arr, bucket->next_pos);
	BUCKET_TAG *const prev = POS_TO_PTR(hm->arr, bucket->prev_pos);

#ifdef CELLAR_COALESCED_HASHING /* clang-format off */
	ASSERT_POINTER_BOUNDS(next, hm->arr, hm->arr + hm->capacity + hm->cellar.capacity - 1);
	ASSERT_POINTER_BOUNDS(prev, hm->arr, hm->arr + hm->capacity + hm->cellar.capacity - 1);
#else  /* clang-format on */
	ASSERT_POINTER_BOUNDS(next, hm->arr, hm->arr + hm->capacity - 1);
	ASSERT_POINTER_BOUNDS(prev, hm->arr, hm->arr + hm->capacity - 1);
#endif /* CELLAR_COALESCED_HASHING */
	if (next)
		next->prev_pos = bucket->prev_pos;

	if (prev)
		prev->next_pos = bucket->next_pos;

	bucket->next_pos = 0;
	bucket->prev_pos = 0;
}

/*!
 * @brief insert a `Bucket` after another Bucket in the list.
 *
 * @param hm non-null pointer to the `HashMap` with all the Buckets.
 * @param bucket non-null pointer to the Bucket to insert.
 * @param here pointer to the Bucket to insert after.
 */
static void BUCKET_METHOD(insert_after)(
	HASHMAP_TAG *const restrict hm, BUCKET_TAG *const restrict bucket,
	BUCKET_TAG *const restrict here
)
{
	assert(bucket != NULL);
	const size_t bucket_pos = PTR_TO_POS(hm->arr, bucket);
	assert(bucket_pos > 0 && "position out of bounds");
#ifdef CELLAR_COALESCED_HASHING
	ASSERT_MAX_POSITION(bucket_pos, hm->capacity + hm->cellar.capacity);
#else
	ASSERT_MAX_POSITION(bucket_pos, hm->capacity);
#endif /* CELLAR_COALESCED_HASHING */

	if (!here)
	{
#ifdef EMPTY_BUCKET_STACK
		assert(hm->top_pos == 0 && hm->bottom_pos == 0 && "stack not empty");
		hm->top_pos = bucket_pos;
		hm->bottom_pos = bucket_pos;
#else
		assert(here != NULL);
#endif /* EMPTY_BUCKET_STACK */
		return;
	}

	BUCKET_TAG *const next = POS_TO_PTR(hm->arr, here->next_pos);
	const size_t here_pos = PTR_TO_POS(hm->arr, here);
	assert(here_pos > 0 && "position out of bounds");
#ifdef CELLAR_COALESCED_HASHING /* clang-format off */
	ASSERT_POINTER_BOUNDS(next, hm->arr, hm->arr + hm->capacity + hm->cellar.capacity - 1);
	ASSERT_MAX_POSITION(here_pos, hm->capacity + hm->cellar.capacity);
#else  /* clang-format on */
	ASSERT_POINTER_BOUNDS(next, hm->arr, hm->arr + hm->capacity - 1);
	ASSERT_MAX_POSITION(here_pos, hm->capacity);
#endif /* CELLAR_COALESCED_HASHING */

	bucket->next_pos = here->next_pos;
	bucket->prev_pos = here_pos;
	here->next_pos = bucket_pos;
	if (next)
		next->prev_pos = bucket_pos;

#ifdef EMPTY_BUCKET_STACK
	if (hm->bottom_pos == here_pos)
		hm->bottom_pos = bucket_pos;
#endif /* EMPTY_BUCKET_STACK */
}

/*!
 * @brief insert a `Bucket` before another Bucket in the list.
 *
 * @param hm pointer to the `HashMap` with all the Buckets.
 * @param bucket pointer to the Bucket to insert.
 * @param here pointer to the Bucket to insert before.
 */
void BUCKET_METHOD(insert_before)(
	HASHMAP_TAG *const restrict hm, BUCKET_TAG *const restrict bucket,
	BUCKET_TAG *const restrict here
)
{
	assert(bucket != NULL);
	const size_t bucket_pos = PTR_TO_POS(hm->arr, bucket);
	assert(bucket_pos > 0 && "position out of bounds");
#ifdef CELLAR_COALESCED_HASHING
	ASSERT_MAX_POSITION(bucket_pos, hm->capacity + hm->cellar.capacity);
#else
	ASSERT_MAX_POSITION(bucket_pos, hm->capacity);
#endif /* CELLAR_COALESCED_HASHING */

	if (!here)
	{
#ifdef EMPTY_BUCKET_STACK
		assert(hm->top_pos == 0 && hm->bottom_pos == 0 && "stack not empty");
		hm->top_pos = bucket_pos;
		hm->bottom_pos = bucket_pos;
#else
		assert(here != NULL);
#endif /* EMPTY_BUCKET_STACK */
		return;
	}

	BUCKET_TAG *const prev = POS_TO_PTR(hm->arr, here->prev_pos);
	const size_t here_pos = PTR_TO_POS(hm->arr, here);
	assert(here_pos > 0 && "position out of bounds");
#ifdef CELLAR_COALESCED_HASHING /* clang-format off */
	ASSERT_POINTER_BOUNDS(prev, hm->arr, hm->arr + hm->capacity + hm->cellar.capacity - 1);
	ASSERT_MAX_POSITION(here_pos, hm->capacity + hm->cellar.capacity);
#else  /* clang-format on */
	ASSERT_POINTER_BOUNDS(prev, hm->arr, hm->arr + hm->capacity - 1);
	ASSERT_MAX_POSITION(here_pos, hm->capacity);
#endif /* CELLAR_COALESCED_HASHING */

	bucket->next_pos = here_pos;
	bucket->prev_pos = here->prev_pos;
	here->prev_pos = bucket_pos;
	if (prev)
		prev->next_pos = bucket_pos;

#ifdef EMPTY_BUCKET_STACK
	if (hm->top_pos == here_pos)
		hm->top_pos = bucket_pos;
#endif /* EMPTY_BUCKET_STACK */
}

/*!
 * @brief search a `HashMap` for an empty `Bucket`.
 *
 * The Cellar if available will be searched first.
 *
 * @param hm the HashMap to search.
 * @returns pointer to an empty Bucket, NULL if none.
 */
static BUCKET_TAG *HASHMAP_METHOD(get_empty)(HASHMAP_TAG *const hm)
{
	BUCKET_TAG *empty_bucket = NULL;
#ifdef EMPTY_BUCKET_STACK

	empty_bucket = POS_TO_PTR(hm->arr, hm->top_pos);
	#ifdef CELLAR_COALESCED_HASHING /* clang-format off */
	ASSERT_POINTER_BOUNDS(empty_bucket, hm->arr, hm->arr + hm->capacity + hm->cellar.capacity - 1);
	#else
	ASSERT_POINTER_BOUNDS(empty_bucket, hm->arr, hm->arr + hm->capacity - 1);
	#endif /* CELLAR_COALESCED_HASHING */ /* clang-format on */
	if (empty_bucket)
		BUCKET_METHOD(unlink)(hm, empty_bucket);
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
		--i;
		if (BUCKET_METHOD(islive)(hm->arr[i]) == false)
		{
			empty_bucket = &hm->arr[i];
			break;
		}
	}
#endif     /* EMPTY_BUCKET_STACK */

	if (empty_bucket)
		*empty_bucket = (BUCKET_TAG){0};

	return (empty_bucket);
}

/*!
 * @brief return pointer to the last `Bucket` in the the linked list.
 *
 * @param hm pointer to the HashMap holding the Buckets.
 * @param head pointer to the first Bucket in the list.
 * @returns pointer to the last Bucket in the list.
 */
static BUCKET_TAG *BUCKET_METHOD(list_tail)(
	HASHMAP_TAG *const restrict hm, BUCKET_TAG *const head
)
{
	BUCKET_TAG *walk = head;

	while (walk->next_pos)
	{
		walk = POS_TO_PTR(hm->arr, walk->next_pos);
#ifdef CELLAR_COALESCED_HASHING /* clang-format off */
		ASSERT_POINTER_BOUNDS(walk, hm->arr, hm->arr + hm->capacity + hm->cellar.capacity - 1);
#else
		ASSERT_POINTER_BOUNDS(walk, hm->arr, hm->arr + hm->capacity - 1);
#endif /* CELLAR_COALESCED_HASHING */ /* clang-format on */
	}

	return (walk);
}

/*!
 * @brief insert a `Bucket` into a `HashMap`.
 *
 * @param hm non-null pointer to the HashMap.
 *
 * @invariant arguments are valid.
 * @invariant HashMap has atleast one empty Bucket.
 *
 * @returns pointer to the inserted Bucket, ideally should never fail.
 */
static BUCKET_TAG *
HASHMAP_METHOD(place)(HASHMAP_TAG *const hm, BUCKET_TAG bucket)
{
	assert(hm->used < hm->capacity);
	bucket.next_pos = 0;
	bucket.prev_pos = 0;
	BUCKET_TAG *const head = &hm->arr[FOLD(bucket.hash, hm->capacity)];

	if (BUCKET_METHOD(islive)(*head) == false)
	{
#ifdef EMPTY_BUCKET_STACK
		/* Bucket must be removed from list of empty buckets. */
		BUCKET_METHOD(unlink)(hm, head);
#endif /* EMPTY_BUCKET_STACK */
		*head = bucket;
		hm->used++;
		return (head);
	}

	/* Getting an empty Bucket should never fail. */

	BUCKET_TAG *const restrict empty = HASHMAP_METHOD(get_empty)(hm);
	BUCKET_TAG *const restrict tail = BUCKET_METHOD(list_tail)(hm, head);

	*empty = bucket;
	BUCKET_METHOD(insert_after)(hm, empty, tail);
#ifdef CELLAR_COALESCED_HASHING
	if (hm->cellar.capacity > 0 && empty >= &hm->arr[hm->capacity])
		hm->cellar.used++;
	else
#endif /* CELLAR_COALESCED_HASHING */
		hm->used++;

	return (empty);
}

/*!
 * @brief double the capacity of a `HashMap` if needed.
 *
 * @param hm non-null pointer to the HashMap to grow.
 * @returns pointer to the doubled HashMap, NULL on failure.
 */
static HASHMAP_TAG *HASHMAP_METHOD(double_capacity)(HASHMAP_TAG *const hm)
{
	if (hm->used <= HASHMAP_MAX_LOAD_FACTOR * hm->capacity)
		return (hm);

	return (HASHMAP_METHOD(grow)(hm, hm->capacity * 2));
}

/*****************************************************************************/

/**
 * @brief free memory allocated to a HashMap.
 *
 * @param hm pointer to the HashMap to delete.
 * @param data_free pointer to a function that can free the data in the buckets.
 * @returns NULL always.
 */
void *HASHMAP_METHOD(delete)(
	HASHMAP_TAG *const restrict hm,
	HM_CONCAT(free_mem_, HASHMAP_UNIQUE_SUFFIX) * data_free
)
{
	if (HASHMAP_METHOD(isvalid)(hm) == false)
		return (xfree(hm));

	size_t i = hm->capacity;

#ifdef CELLAR_COALESCED_HASHING
	i += hm->cellar.capacity;
#endif /* CELLAR_COALESCED_HASHING */
	while (i > 0)
	{
		--i;
		BUCKET_TAG *const restrict bucket = &hm->arr[i];

		if (BUCKET_METHOD(islive)(*bucket) == false)
			continue;

		bucket->key = u8mem_delete(bucket->key);
		if (data_free)
			data_free(bucket->data);

		*bucket = (BUCKET_TAG){0};
	}

	*hm = (HASHMAP_TAG){0};
	return (xfree(hm));
}

/**
 * @brief allocate and initialise memory for a `HashMap`.
 *
 * @param capacity number of buckets the HashMap will contain.
 * @returns pointer to the hash map success, NULL on failure.
 */
HASHMAP_TAG *HASHMAP_METHOD(new)(len_ty capacity)
{
	if (capacity < 1)
		return (NULL);

#ifdef POWER2_ROUNDUP_FUNC
	capacity = power2_roundup(capacity);
#endif /* POWER2_ROUNDUP_FUNC */

#ifdef CELLAR_COALESCED_HASHING
	/** https://en.wikipedia.org/wiki/Coalesced_hashing#The_cellar
	 * Optimum cellar size should be 14% of the total number of buckets,
	 * including the cellar.
	 */
	const len_ty cellar_cap = (capacity * 14) / 86;
#else
	const len_ty cellar_cap = 0;
#endif /* CELLAR_COALESCED_HASHING */
	const size_t size = sizeof(BUCKET_TAG) * (capacity + cellar_cap);

	/* overflow errors. */
	if (SIZE_MAX - size < sizeof(HASHMAP_TAG) ||
		size / sizeof(BUCKET_TAG) != (size_t)(capacity + cellar_cap))
		return (NULL);

#ifdef EMPTY_BUCKET_STACK
	HASHMAP_TAG *const map = xmalloc(sizeof(*map) + size);
#else
	HASHMAP_TAG *const map = xcalloc(1, sizeof(*map) + size);
#endif /* EMPTY_BUCKET_STACK */

	if (!map)
		return (NULL);

	*map = (HASHMAP_TAG){.capacity = capacity};
#ifdef CELLAR_COALESCED_HASHING
	map->cellar.capacity = cellar_cap;
#endif /* CELLAR_COALESCED_HASHING */
#ifdef EMPTY_BUCKET_STACK
	/* Initialising the stack of free Buckets. */

	size_t prev_pos = 0, i = capacity + cellar_cap;

	map->bottom_pos = 1;
	map->top_pos = i;
	while (i > 0)
	{
		const size_t curr_pos = i--;

		map->arr[i] = (BUCKET_TAG){
			.next_pos = curr_pos - 1,
			.prev_pos = prev_pos,
		};
		prev_pos = curr_pos;
	}

#endif /* EMPTY_BUCKET_STACK */
	return (map);
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
HASHMAP_TAG *HASHMAP_METHOD(dup)(
	const HASHMAP_TAG *const hm,
	HM_CONCAT(duplicate_, HASHMAP_UNIQUE_SUFFIX) data_dup,
	HM_CONCAT(free_mem_, HASHMAP_UNIQUE_SUFFIX) data_free
)
{
	if (HASHMAP_METHOD(isvalid)(hm) == false || (data_dup && !data_free))
		return (NULL);

	HASHMAP_TAG *const restrict cpy = HASHMAP_METHOD(new)(hm->capacity);

	if (!cpy)
		return (NULL);

	*cpy = *hm;
	len_ty i = hm->capacity;

#ifdef CELLAR_COALESCED_HASHING
	i += hm->capacity;
#endif /* CELLAR_COALESCED_HASHING */
	while (i > 0)
	{
		i--;
		/* No need to update next and previous pointers as next and prev are */
		/* positions relative to the beginning of the bucket array. */
		cpy->arr[i] = hm->arr[i];
		if (BUCKET_METHOD(islive)(hm->arr[i]) == true)
		{
			const u8mem *const key = hm->arr[i].key;
			const HASHMAP_DATATYPE data = hm->arr[i].data;

			cpy->arr[i].key = u8mem_new(key->buf, key->len);
			if (!cpy->arr[i].key)
				return (HASHMAP_METHOD(delete)(cpy, data_free));

			if (data_dup && data_dup(&cpy->arr[i].data, data) == false)
				return (HASHMAP_METHOD(delete)(cpy, data_free));
		}
#ifdef EMPTY_BUCKET_STACK
		else
		{ /* Nothing to be done. */
		}
#endif /* EMPTY_BUCKET_STACK */
	}

	return (cpy);
}

/*!
 * @brief grow the capacity of a `HashMap` to the given capacity.
 *
 * @param hm pointer to the HashMap.
 * @param capacity the new capacity.
 * @returns pointer to the expanded HashMap, NULL on failure.
 */
HASHMAP_TAG *HASHMAP_METHOD(grow)(HASHMAP_TAG *const hm, const len_ty capacity)
{
	if (HASHMAP_METHOD(isvalid)(hm) == false)
		return (NULL);

	if (capacity <= hm->capacity)
		return (hm);

#ifdef POWER2_ROUNDUP_FUNC
	HASHMAP_TAG *const restrict new_map =
		HASHMAP_METHOD(new)(power2_roundup(capacity));
#else
	HASHMAP_TAG *const restrict new_map = HASHMAP_METHOD(new)(capacity);
#endif /* POWER2_ROUNDUP_FUNC */

	if (!new_map)
		return (NULL);

	len_ty i = hm->capacity;

#ifdef CELLAR_COALESCED_HASHING
	i += hm->cellar.capacity;
#endif /* CELLAR_COALESCED_HASHING */
	while (i > 0)
	{
		i--;
		if (BUCKET_METHOD(islive)(hm->arr[i]) == false)
			continue;

		/* slots should not run out as new HashMap should have */
		/* more capacity than the old one. */
		HASHMAP_METHOD(place)(new_map, hm->arr[i]);
		hm->arr[i] = (BUCKET_TAG){0};
	}

	HASHMAP_METHOD(delete)(hm, NULL);
	return (new_map);
}

/*!
 * @brief calculate the hash value of a given key.
 *
 * @param dest address to store the hash.
 * @param key block of memory to hash.
 * @returns true on success, false otherwise.
 */
bool HASHMAP_METHOD(hash)(hash_ty *const dest, const u8mem key)
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
 * @brief search for a `Bucket` with the same key as the given key.
 *
 * @param hm the HashMap to search.
 * @param key the key to search for.
 * @returns pointer to the data in the Bucket with the same key, NULL on
 * failure
 */
HASHMAP_DATATYPE *
HASHMAP_METHOD(search)(HASHMAP_TAG *const hm, const u8mem key)
{
	if (HASHMAP_METHOD(isvalid)(hm) == false)
		return (NULL);

	hash_ty hash;

	if (HASHMAP_METHOD(hash)(&hash, key) == false)
		return (NULL);

	BUCKET_TAG *const bucket = HASHMAP_METHOD(search_list)(hm, hash, key);

	if (bucket)
		return (&bucket->data);

	return (NULL);
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
HASHMAP_DATATYPE *HASHMAP_METHOD(insert)(
	HASHMAP_TAG *restrict *const hm, const u8mem key, HASHMAP_DATATYPE data
)
{
	if (!hm || HASHMAP_METHOD(isvalid)(*hm) == false || key.len < 1 ||
		!key.buf)
		return (NULL);

	hash_ty hash;

	if (HASHMAP_METHOD(hash)(&hash, key) == false)
		return (NULL);

	HASHMAP_TAG *restrict map = *hm;
	BUCKET_TAG *bucket = HASHMAP_METHOD(search_list)(map, hash, key);

	if (bucket)
	{
		bucket->data = data;
		return (&bucket->data);
	}

	u8mem *const restrict key_dup = u8mem_new(key.buf, key.len);

	if (!key_dup)
		return (NULL);

	map = HASHMAP_METHOD(double_capacity)(map);
	if (!map)
		return (u8mem_delete(key_dup));

	/* should never fail. */
	bucket = HASHMAP_METHOD(place)(
		map, (BUCKET_TAG){.hash = hash, .key = key_dup, .data = data}
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
bool HASHMAP_METHOD(remove)(
	HASHMAP_TAG *restrict hm, HASHMAP_DATATYPE *const restrict dest,
	const u8mem key
)
{
	if (HASHMAP_METHOD(isvalid)(hm) == false || key.len < 1 || !key.buf)
		return (false);

	hash_ty hash;

	if (HASHMAP_METHOD(hash)(&hash, key) == false)
		return (false);

	BUCKET_TAG *removed = HASHMAP_METHOD(search_list)(hm, hash, key);

	if (!removed) /* key does not exist. */
		return (false);

	if (dest)
		*dest = removed->data;

	BUCKET_TAG *walk = POS_TO_PTR(hm->arr, removed->next_pos);
#ifdef CELLAR_COALESCED_HASHING
	ASSERT_POINTER_BOUNDS(walk, hm->arr, hm->arr + hm->capacity + hm->cellar.capacity - 1);
#else
	ASSERT_POINTER_BOUNDS(walk, hm->arr, hm->arr + hm->capacity - 1);
#endif /* CELLAR_COALESCED_HASHING */

	BUCKET_METHOD(unlink)(hm, removed);
	*removed = (BUCKET_TAG){.key = u8mem_delete(removed->key)};
	while (walk)
	{
		BUCKET_TAG *const new_spot = &hm->arr[FOLD(walk->hash, hm->capacity)];
		BUCKET_TAG *const next = POS_TO_PTR(hm->arr, walk->next_pos);
#ifdef CELLAR_COALESCED_HASHING /* clang-format off */
		ASSERT_POINTER_BOUNDS(next, hm->arr, hm->arr + hm->capacity + hm->cellar.capacity - 1);
#else
		ASSERT_POINTER_BOUNDS(next, hm->arr, hm->arr + hm->capacity - 1);
#endif /* CELLAR_COALESCED_HASHING */ /* clang-format on */

		walk->next_pos = 0;
		BUCKET_METHOD(unlink)(hm, walk);
		if (BUCKET_METHOD(islive)(*new_spot) == true)
		{
			BUCKET_TAG *const tail = BUCKET_METHOD(list_tail)(hm, new_spot);

			BUCKET_METHOD(insert_after)(hm, walk, tail);
		}
		else
		{
			*new_spot = *walk;
			removed = walk;
			*removed = (BUCKET_TAG){0};
		}

		walk = next;
	}

#ifdef EMPTY_BUCKET_STACK
	#ifdef CELLAR_COALESCED_HASHING
	if (removed < hm->arr + hm->capacity)
	{
		BUCKET_TAG *const bottom = POS_TO_PTR(hm->arr, hm->bottom_pos);
		#ifdef CELLAR_COALESCED_HASHING /* clang-format off */
		ASSERT_POINTER_BOUNDS(bottom, hm->arr, hm->arr + hm->capacity + hm->cellar.capacity - 1);
		#else
		ASSERT_POINTER_BOUNDS(bottom, hm->arr, hm->arr + hm->capacity - 1);
		#endif /* CELLAR_COALESCED_HASHING */ /* clang-format on */

		BUCKET_METHOD(insert_after)(hm, removed, bottom);
	}
	else
	#else
	{
		BUCKET_TAG *const top = POS_TO_PTR(hm->arr, hm->top_pos);
		#ifdef CELLAR_COALESCED_HASHING /* clang-format off */
		ASSERT_POINTER_BOUNDS(top, hm->arr, hm->arr + hm->capacity + hm->cellar.capacity - 1);
		#else
		ASSERT_POINTER_BOUNDS(top, hm->arr, hm->arr + hm->capacity - 1);
		#endif /* CELLAR_COALESCED_HASHING */ /* clang-format on */

		BUCKET_METHOD(insert_before)(hm, removed, top);
	}

	#endif /* CELLAR_COALESCED_HASHING */
#endif /* EMPTY_BUCKET_STACK */
#ifdef CELLAR_COALESCED_HASHING
	if (removed >= hm->arr + hm->capacity)
		hm->cellar.used--;
	else
#endif /* CELLAR_COALESCED_HASHING */
		hm->used--;

	return (true);
}

/*!
 * @brief return the string representation of a `Bucket`.
 *
 * @param bucket the Bucket.
 * @param data_tostr pointer to a function that can stringify the data in
 * the Bucket.
 * @returns pointer to the buffer, NULL on error.
 */
static char *BUCKET_METHOD(tostr)(
	const BUCKET_TAG bucket,
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
char *HASHMAP_METHOD(tostr)(
	const HASHMAP_TAG *const restrict hm,
	HM_CONCAT(stringify_data_, HASHMAP_UNIQUE_SUFFIX) * data_tostr
)
{
	if (HASHMAP_METHOD(isvalid)(hm) == false || !data_tostr)
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
		if (BUCKET_METHOD(islive)(hm->arr[i]) == false)
			continue;

		char *const restrict bucket_str =
			BUCKET_METHOD(tostr)(hm->arr[i], data_tostr);

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

#undef HASHMAP_UNIQUE_SUFFIX
#undef HASHMAP_DATATYPE

#undef HM_CONCAT0
#undef HM_CONCAT
#undef HASHMAP_TAG
#undef BUCKET_TAG
#undef CELLAR_TAG
#undef HASHMAP_METHOD
#undef BUCKET_METHOD

#undef FOLD
#undef POS_TO_PTR
#undef PTR_TO_POS
#undef ASSERT_MAX_POSITION
#undef ASSERT_POINTER_BOUNDS
