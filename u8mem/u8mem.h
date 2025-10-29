#ifndef DS_U8MEM
#define DS_U8MEM

#include "compiler_attributes_macros.h"
#include "len_type.h"

/*!
 * @brief block of memory.
 */
typedef struct u8mem
{
	/*! @public length of the memory block. */
	len_ty len;
	/*! @public the block of memory. */
	unsigned char *restrict buf;
} u8mem;

/* alloc */

void *u8mem_delete(u8mem *const restrict m);
u8mem *u8mem_new(
	const unsigned char *const restrict mem, const len_ty len
) _malloc _malloc_free(u8mem_delete);

int u8mem_compare(const u8mem a, const u8mem b);
char *u8mem_tostr(const u8mem m) _malloc;

#endif /* DS_U8MEM */
