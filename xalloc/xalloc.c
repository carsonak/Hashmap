/*!
 * @file
 * @brief memory manipulation functions.
 */

#include <stdio.h>   // perror
#include <string.h>  // strncpy, strlen

#include "xalloc.h"

/*!
 * @brief allocate memory with malloc and report errors.
 *
 * @param[in] size size in bytes to allocate.
 *
 * @return a pointer to the memory area, NULL on failure.
 */
void *xmalloc(const len_ty size)
{
	if (size < 0)
	{
		fprintf(stderr, "ERROR:xmalloc: negative size %" PRI_len, size);
		return (NULL);
	}

	void *const restrict ptr = malloc(size);

	if (!ptr && size > 0)
		perror("ERROR:xmalloc");

	return (ptr);
}

/*!
 * @brief allocate memory with calloc and report errors.
 *
 * @param[in] items number of items to allocate memory for.
 * @param[in] sizeof_item the size in bytes of each item.
 *
 * @return a pointer to the memory area, NULL on failure.
 */
void *xcalloc(const len_ty items, const len_ty sizeof_item)
{
	if (items < 0)
	{
		fprintf(stderr, "ERROR:xcalloc: negative parameter %" PRI_len, items);
		return (NULL);
	}

	if (sizeof_item < 0)
	{
		fprintf(
			stderr, "ERROR:xcalloc: negative parameter %" PRI_len, sizeof_item
		);
		return (NULL);
	}

	void *const restrict ptr = calloc(items, sizeof_item);

	if (!ptr && ((size_t)items * sizeof_item) > 0)
		perror("ERROR:xcalloc");

	return (ptr);
}

/*!
 * @brief resize memory with realloc and report errors.
 *
 * @param[in] nullable_ptr pointer to an allocated block.
 * @param[in] size size in bytes to resize to.
 *
 * @return pointer to the resized memory area, NULL on failure.
 */
void *xrealloc(void *nullable_ptr, const len_ty size)
{
	if (size < 0)
	{
		fprintf(stderr, "ERROR:xrealloc: negative size %" PRI_len, size);
		return (NULL);
	}

	nullable_ptr = realloc(nullable_ptr, size);
	if (!nullable_ptr && size > 0)
		perror("ERROR:xrealloc");

	return (nullable_ptr);
}

/*!
 * @brief resize memory with xrealloc but free original memory on error.
 *
 * @param[in] nullable_ptr pointer to an allocated block.
 * @param[in] size size in bytes to resize to.
 *
 * @return pointer to the resized memory area, NULL on failure.
 *
 */
void *xrealloc_free_on_fail(void *nullable_ptr, const len_ty size)
{
	void *const restrict ptr = realloc(nullable_ptr, size);

	if (!ptr && size)
		free(nullable_ptr);

	return (ptr);
}

/*!
 * @brief duplicate a string and report errors.
 *
 * @param[in] str the string to duplicate.
 *
 * @return pointer to the duplicated string, NULL on failure.
 */
char *xstrdup(char const *const str)
{
	if (!str)
		return (NULL);

	const size_t s_size = strlen(str) + 1;
	char *const restrict s = malloc(sizeof(*s) * s_size);

	if (str)
		strncpy(s, str, s_size);

	return (s);
}

/*!
 * @brief free a pointer, return NULL.
 *
 * @param[in] freeable_ptr a freeable pointer.
 *
 * @return NULL always.
 */
void *xfree(void *const freeable_ptr)
{
	free(freeable_ptr);
	return (NULL);
}
