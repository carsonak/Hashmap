#ifndef HASHMAP_UNIQUE_SUFFIX
#error "Missing definition for `HASHMAP_UNIQUE_SUFFIX`."
#endif /* HASHMAP_UNIQUE_SUFFIX */

#ifndef HASHMAP_DATATYPE
#error "Missing definition for `HASHMAP_DATATYPE`."
#endif /* HASHMAP_DATATYPE */

#include <stdbool.h> /* bool */
#include <stdint.h>  /* fixed width types */

#include "len_type.h"
#include "u8mem/u8mem.h"

#define HM_CONCAT0(tok0, tok1) tok0##tok1
#define HM_CONCAT(tok0, tok1) HM_CONCAT0(tok0, tok1)

#define HASHMAP_STRUCT_TAG HM_CONCAT(HashMap_, HASHMAP_UNIQUE_SUFFIX)
#define BUCKET_STRUCT_TAG HM_CONCAT(Bucket_, HASHMAP_UNIQUE_SUFFIX)

#ifndef DS_HASHMAP_HASHTYPE
#define DS_HASHMAP_HASHTYPE
typedef uint32_t hash_ty;
#endif /* DS_HASHMAP_HASHTYPE */

/*!
 * @brief type that holds details of a HashMap entry.
 */
struct BUCKET_STRUCT_TAG {
  /*! @public data stored in the HashMap. */
  HASHMAP_DATATYPE data;
  /*! @protected the hash of the key. */
  hash_ty hash;
  /*! @protected unique key of the entry. */
  u8mem *restrict key;
  /*! @protected position to the previous colliding Bucket. */
  unsigned int prev_pos;
  /*! @protected position to the next colliding Bucket. */
  unsigned int next_pos;
};

// #define CELLAR_COALESCED_HASHING

#ifdef CELLAR_COALESCED_HASHING
#define CELLAR_STRUCT_TAG HM_CONCAT(Cellar_, HASHMAP_UNIQUE_SUFFIX)

/*!
 * @brief space used to hold collided Buckets of a HashMap.
 *
 * https://en.wikipedia.org/wiki/Coalesced_hashing#The_cellar
 */
struct CELLAR_STRUCT_TAG {
  /*! @public total number of slots in the cellar. */
  len_ty capacity;
  /*! @protected number of used Buckets in the cellar. */
  len_ty used;
};
#endif /* CELLAR_COALESCED_HASHING */

// #define EMPTY_BUCKET_STACK

/*!
 * @brief a hash table type.
 */
struct HASHMAP_STRUCT_TAG {
  /*! @public total number of Buckets in the HashMap. */
  len_ty capacity;
  /*! @public  number of used Buckets. */
  len_ty used;
#ifdef CELLAR_COALESCED_HASHING
  /*! @protected cellar for handling colliding buckets. */
  struct CELLAR_STRUCT_TAG cellar;
#endif /* CELLAR_COALESCED_HASHING */
#ifdef EMPTY_BUCKET_STACK
  /*! @protected position of the bucket at the top of the stack. */
  unsigned int top_pos;
#endif /* EMPTY_BUCKET_STACK */
  /*! @protected array of Buckets. */
  struct BUCKET_STRUCT_TAG arr[];
};

#define HASHMAP_MAX_LOAD_FACTOR 0.95

#undef HASHMAP_UNIQUE_SUFFIX
#undef HASHMAP_DATATYPE

#undef HM_CONCAT0
#undef HM_CONCAT

#undef HASHMAP_STRUCT_TAG
#undef BUCKET_STRUCT_TAG
#undef CELLAR_STRUCT_TAG
