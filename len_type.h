#ifndef DS_LEN_TYPE_H
#define DS_LEN_TYPE_H

#include <stddef.h>  // ptrdiff_t
#include <stdint.h>  // PTRDIFF_MAX

/*! for lengths and counting. */
typedef ptrdiff_t len_ty;

/* maximums and minimums. */

#define LEN_TY_max PTRDIFF_MAX
#define LEN_TY_min PTRDIFF_MIN

/* *printf conversion specifiers. */

#define PRI_len "td"

#endif  // DS_LEN_TYPE_H
