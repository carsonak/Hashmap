#ifndef HASHMAP_UNIQUE_SUFFIX
#error "Missing definition for `HASHMAP_UNIQUE_SUFFIX`."
#endif /* HASHMAP_UNIQUE_SUFFIX */

#ifndef HASHMAP_DATATYPE
#error "Missing definition for `HASHMAP_DATATYPE`."
#endif /* HASHMAP_DATATYPE */

#include <stdbool.h> /* bool */
#include <stdint.h>  /* fixed width types */

#include "compiler_attributes_macros.h"
#include "len_type.h"
#include "u8mem/u8mem.h"

#define COMMON_CALLBACKS_UNIQUE_SUFFIX HASHMAP_UNIQUE_SUFFIX
#define COMMON_CALLBACKS_DATATYPE HASHMAP_DATATYPE
#include "common_callback_types.h"

#define HM_CONCAT0(tok0, tok1) tok0##tok1
#define HM_CONCAT(tok0, tok1) HM_CONCAT0(tok0, tok1)

#define HASHMAP_STRUCT_TAG HM_CONCAT(HashMap_, HASHMAP_UNIQUE_SUFFIX)
#define BUCKET_STRUCT_TAG HM_CONCAT(Bucket_, HASHMAP_UNIQUE_SUFFIX)
#define CELLAR_STRUCT_TAG HM_CONCAT(Cellar_, HASHMAP_UNIQUE_SUFFIX)

#define HASHMAP_METHODNAME(name)                                               \
  HM_CONCAT(HM_CONCAT(hm_, HASHMAP_UNIQUE_SUFFIX), HM_CONCAT(_, name))

#ifndef DS_HASHMAP_HASHTYPE
#define DS_HASHMAP_HASHTYPE
typedef uint64_t hash_ty;
#endif /* DS_HASHMAP_HASHTYPE */
typedef struct BUCKET_STRUCT_TAG BUCKET_STRUCT_TAG;
typedef struct CELLAR_STRUCT_TAG CELLAR_STRUCT_TAG;
typedef struct HASHMAP_STRUCT_TAG HASHMAP_STRUCT_TAG;

/* alloc */

HASHMAP_STRUCT_TAG *HASHMAP_METHODNAME(new)(len_ty capacity);
void *HASHMAP_METHODNAME(delete)(HASHMAP_STRUCT_TAG *const restrict hm,
                                 HM_CONCAT(free_mem_, HASHMAP_UNIQUE_SUFFIX) *
                                     data_free);
HASHMAP_STRUCT_TAG *HASHMAP_METHODNAME(grow)(HASHMAP_STRUCT_TAG *const hm,
                                             const len_ty capacity);
HASHMAP_DATATYPE *
    HASHMAP_METHODNAME(insert)(HASHMAP_STRUCT_TAG *restrict *const hm,
                               const u8mem key, HASHMAP_DATATYPE data);
HASHMAP_STRUCT_TAG *HASHMAP_METHODNAME(copy)(
    const HASHMAP_STRUCT_TAG *const restrict hm,
    HM_CONCAT(duplicate_, HASHMAP_UNIQUE_SUFFIX) data_dup,
    HM_CONCAT(free_mem_, HASHMAP_UNIQUE_SUFFIX) data_free);

bool HASHMAP_METHODNAME(hash)(hash_ty *const dest, const u8mem key);
HASHMAP_DATATYPE *HASHMAP_METHODNAME(search)(HASHMAP_STRUCT_TAG *const hm,
                                             const u8mem key);
bool HASHMAP_METHODNAME(remove)(HASHMAP_STRUCT_TAG *restrict hm,
                                HASHMAP_DATATYPE *const restrict dest,
                                const u8mem key);

char *HASHMAP_METHODNAME(tostr)(const HASHMAP_STRUCT_TAG *const restrict hm,
                                HM_CONCAT(stringify_data_,
                                          HASHMAP_UNIQUE_SUFFIX) *
                                    data_tostr);

#undef HASHMAP_UNIQUE_SUFFIX
#undef HASHMAP_DATATYPE

#undef HM_CONCAT0
#undef HM_CONCAT

#undef HASHMAP_STRUCT_TAG
#undef BUCKET_STRUCT_TAG
#undef CELLAR_STRUCT_TAG

#undef HASHMAP_METHODNAME
