#ifndef HASHMAP_UNIQUE_SUFFIX
	#error "Missing definition for `HASHMAP_UNIQUE_SUFFIX`."
#endif /* HASHMAP_UNIQUE_SUFFIX */

#ifndef HASHMAP_DATATYPE
	#error "Missing definition for `HASHMAP_DATATYPE`."
#endif /* HASHMAP_DATATYPE */

#include <stdbool.h> /* bool */
#include <stdint.h>  /* fixed width types */

#include "common_callback_types.h"
#include "compiler_attributes_macros.h"
#include "len_type.h"
#include "u8mem/u8mem.h"

#define COMMON_CALLBACKS_UNIQUE_SUFFIX HASHMAP_UNIQUE_SUFFIX
#define COMMON_CALLBACKS_DATATYPE HASHMAP_DATATYPE
#include "common_generic_callback_types.h"

/************************* GENERICS BUILDER MACROS ***************************/

#define HM_CONCAT0(tok0, tok1) tok0##tok1
#define HM_CONCAT(tok0, tok1) HM_CONCAT0(tok0, tok1)

#define HASHMAP_TAG HM_CONCAT(HashMap_, HASHMAP_UNIQUE_SUFFIX)

#define HASHMAP_METHOD(name)                                                  \
	HM_CONCAT(HM_CONCAT(hm_, HASHMAP_UNIQUE_SUFFIX), HM_CONCAT(_, name))

/*****************************************************************************/

typedef struct HASHMAP_TAG HASHMAP_TAG;
#ifndef DS_HASHMAP_HASHTYPE
	#define DS_HASHMAP_HASHTYPE
typedef uint32_t hash_ty;
#endif /* DS_HASHMAP_HASHTYPE */

/* alloc */

void *HASHMAP_METHOD(delete)(
	HASHMAP_TAG *const restrict hm,
	HM_CONCAT(free_mem_, HASHMAP_UNIQUE_SUFFIX) * data_free
);
HASHMAP_TAG *HASHMAP_METHOD(new)(len_ty capacity) _malloc _malloc_free(
	HASHMAP_METHOD(delete), 1
);
HASHMAP_TAG *HASHMAP_METHOD(dup)(
	const HASHMAP_TAG *const restrict hm,
	HM_CONCAT(duplicate_, HASHMAP_UNIQUE_SUFFIX) data_dup,
	HM_CONCAT(free_mem_, HASHMAP_UNIQUE_SUFFIX) data_free
) _malloc _malloc_free(HASHMAP_METHOD(delete), 1);
HASHMAP_TAG *
	HASHMAP_METHOD(grow)(HASHMAP_TAG *const hm, const len_ty capacity);

HASHMAP_DATATYPE *HASHMAP_METHOD(insert)(
	HASHMAP_TAG *restrict *const hm, const u8mem key, HASHMAP_DATATYPE data
);
bool HASHMAP_METHOD(hash)(hash_ty *const dest, const u8mem key);
HASHMAP_DATATYPE *
	HASHMAP_METHOD(search)(HASHMAP_TAG *const hm, const u8mem key);

bool HASHMAP_METHOD(remove)(
	HASHMAP_TAG *restrict hm, HASHMAP_DATATYPE *const restrict dest,
	const u8mem key
);

char *HASHMAP_METHOD(tostr)(
	const HASHMAP_TAG *const restrict hm,
	HM_CONCAT(stringify_data_, HASHMAP_UNIQUE_SUFFIX) * data_tostr
);

#undef HASHMAP_UNIQUE_SUFFIX
#undef HASHMAP_DATATYPE

#undef HM_CONCAT0
#undef HM_CONCAT
#undef HASHMAP_TAG
#undef HASHMAP_METHOD
