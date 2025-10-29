#ifndef BIGINT_XALLOC_H
#define BIGINT_XALLOC_H

#include "compiler_attributes_macros.h"

#include <stdlib.h>  // *alloc

#include "len_type.h"

void *xfree(void *const freeable_ptr);
/* clang-format off */

void *xcalloc(const len_ty items, const len_ty sizeof_item) _malloc _malloc_free(xfree) _alloc_size(1, 2);
void *xmalloc(const len_ty size) _malloc _malloc_free(xfree) _alloc_size(1);
void *xrealloc(void *nullable_ptr, const len_ty size) _alloc_size(2);
void *xrealloc_free_on_fail(void *nullable_ptr, const len_ty size) _alloc_size(2);
/* clang-format on */

char *xstrdup(char const *const str) _malloc _malloc_free(xfree);

#endif  // BIGINT_XALLOC_H
