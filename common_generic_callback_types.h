#ifndef COMMON_CALLBACKS_UNIQUE_SUFFIX
	#error "Missing definition for `COMMON_CALLBACKS_UNIQUE_SUFFIX`"
#endif

#ifndef COMMON_CALLBACKS_DATATYPE
	#error "Missing definition for `COMMON_CALLBACKS_DATATYPE`"
#endif

#include <stdbool.h> /* bool */

#define CCB_CONCAT0(tok0, tok1) tok0##tok1
#define CCB_CONCAT(tok0, tok1) CCB_CONCAT0(tok0, tok1)

/**
 * @brief a function that duplicates an object.
 *
 * @param dest address to store the duplicated data.
 * @param data the object to be duplicated.
 * @returns true on success, false on failure.
 */
typedef bool(CCB_CONCAT(duplicate_, COMMON_CALLBACKS_UNIQUE_SUFFIX))(
	COMMON_CALLBACKS_DATATYPE *const restrict dest,
	const COMMON_CALLBACKS_DATATYPE data
);

/**
 * @brief a function that frees an object from memory.
 *
 * @param data the object to delete.
 */
typedef void(CCB_CONCAT(free_mem_, COMMON_CALLBACKS_UNIQUE_SUFFIX))(
	COMMON_CALLBACKS_DATATYPE data
);

/**
 * @brief a function that turns an object into a string.
 *
 * @param data the object to stringify.
 * @returns pointer to the string, NULL error.
 */
typedef char *(CCB_CONCAT(stringify_data_, COMMON_CALLBACKS_UNIQUE_SUFFIX))(
	const COMMON_CALLBACKS_DATATYPE data
);

#undef COMMON_CALLBACKS_UNIQUE_SUFFIX
#undef COMMON_CALLBACKS_DATATYPE

#undef CCB_CONCAT0
#undef CCB_CONCAT
