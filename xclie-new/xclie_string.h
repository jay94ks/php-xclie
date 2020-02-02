/*
   +----------------------------------------------------------------------+
   | XCLIE Module for Php7                                                |
   +----------------------------------------------------------------------+
   | License: LGPLv2                                                      |
   | This file is not a part of official distribution of PHP.			  |
   +----------------------------------------------------------------------+
   | Author: Jaehoon Joe <jay94ks@gmail.com>                              |
   | https://github.com/jay94ks/php-xclie                                 |
   +----------------------------------------------------------------------+
*/

#ifndef __XCLIE_STRING_H__
#define __XCLIE_STRING_H__

#ifndef __XCLIE_H__
#include "xclie.h"
#endif

#if !XCLIE_INTERNAL
#	include <string.h>
#endif

XCLIE_STRUCT(xclie_string, struct {
	xclie_gc_entry gc_entry;
	int32_t size;
	char* memory;
});

XCLIE_STRUCT(xclie_string_list, struct {
	xclie_gc_entry gc_entry;
	int32_t size;
	xclie_string* strings;
});


/*
	create new string object.
	if NULL pointer given, this allocate new memory.
*/
XCLIE_API xclie_string* xclie_string_init_gc(xclie_gc_root* gc_root, xclie_string* where);

/*
	create new string object.
	if NULL pointer given, this allocate new memory.
*/
X_INLINE static xclie_string* xclie_string_init(xclie_string* where) {
	return xclie_string_init_gc(nullptr, where);
}

/*
	create new string list object.
	if NULL pointer given, this allocate new memory.
*/
XCLIE_API xclie_string_list* xclie_string_list_init_gc(xclie_gc_root* gc_root, xclie_string_list* where);

/*
	create new string object.
	if NULL pointer given, this allocate new memory.
*/
X_INLINE static xclie_string_list* xclie_string_list_init(xclie_string_list* where) {
	return xclie_string_list_init_gc(nullptr, where);
}

/*
	resize internal string memory.
	on success, this return new size.
	zero on failure.
*/
XCLIE_API int32_t xclie_string_resize(xclie_string* where, int32_t new_size);

/*
	destroy string object.
	if the string memory was allocated by GC, this kill it completely.

	no_delete has specified, gc_entry will not be unregistered and deleted.
	(string memory only deleted)

	calling this function is optional.
	when gc_root destroying, it will kill string object automatically.
*/
XCLIE_API void xclie_string_deinit(xclie_string* where, int32_t no_delete);

/*
	destroy string object.
	if the string memory was allocated by GC, this kill it completely.

	no_delete has specified, gc_entry will not be unregistered and deleted.
	(string memory only deleted)

	calling this function is optional.
	when gc_root destroying, it will kill string object automatically.
*/
XCLIE_API void xclie_string_list_deinit(xclie_string_list* where, int32_t no_delete);

/*
	add string into string list.
	on success this return 1. zero for failure.
*/
XCLIE_API int32_t xclie_string_list_add(xclie_string_list* where, const char* in_string, int32_t size);

/*
	insert string into string list.
	on success this return 1. zero for failure.
*/
XCLIE_API int32_t xclie_string_list_insert(xclie_string_list* where, int32_t index, const char* in_string, int32_t size);

/* remove string on index. */
XCLIE_API int32_t xclie_string_list_remove(xclie_string_list* where, int32_t index, int32_t count);

/*
	add string into string list.
	on success this return 1. zero for failure.
*/
X_INLINE static int32_t xclie_string_list_add_x(xclie_string_list* where, xclie_string* str, int32_t deinit_str) {
	int32_t retVal = xclie_string_list_add(where, str ? str->memory : nullptr, str ? str->size : 0);
	if (deinit_str) { xclie_string_deinit(str, 0); }
	return retVal;
}

/*
	insert string into string list.
	on success this return 1. zero for failure.
*/
X_INLINE static int32_t xclie_string_list_insert_x(xclie_string_list* where, int32_t index, xclie_string* str, int32_t deinit_str) {
	int32_t retVal = xclie_string_list_insert(where, index, str ? str->memory : nullptr, str ? str->size : 0);
	if (deinit_str) { xclie_string_deinit(str, 0); }
	return retVal;
}

/* get C string. */
X_INLINE static char* xclie_c_str(const xclie_string* str) {
	return (str && xclie_gc_is_registered(&str->gc_entry)) ? str->memory : nullptr;
}

/* get length of xclie string. */
X_INLINE static int32_t xclie_strlen(const xclie_string* str) {
	return (str && str->memory) ? str->size : 0;
}

/* concatrate string. */
X_INLINE static xclie_string* xclie_strcat(xclie_string* str, const char* in_str, int32_t size) {
	if (str && xclie_gc_is_registered(&str->gc_entry)) {
		size = size < 0 ? (in_str ? ((int32_t)strlen(in_str)) : 0) : size;

		if (in_str && size) {
			xclie_string_resize(str, str->size + size);
			memcpy(str->memory + (str->size - size), in_str, size);
		}

		return str;
	}

	return nullptr;
}

/* xclie version, strpos() of PHP. */
X_INLINE static int32_t xclie_strpos(xclie_string* haystack, const char* in_str, int32_t offset) {
	const char* pos = 0;

	if (haystack && xclie_gc_is_registered(&haystack->gc_entry)) {
		if (haystack->size) {
			while (offset < 0) {
				offset = haystack->size + offset;
			}

			while (offset > haystack->size) {
				offset -= haystack->size;
			}
			
			if (offset >= 0 && offset < haystack->size) {
				pos = strstr(haystack->memory + offset, in_str);
			}

			if (pos) {
				return (int32_t)((size_t)pos - (size_t)haystack);
			}
		}
	}

	return -1;
}

/* xclie version, substr() of PHP. */
X_INLINE static xclie_string* xclie_substr(xclie_string* str, int32_t offset, int32_t size) {
	xclie_string* new_str = NULL;

	if ((str && xclie_gc_is_registered(&str->gc_entry)) &&
		(new_str = xclie_string_init_gc(str->gc_entry.owner, NULL)) != NULL)
	{
		if (str->size) {
			while (offset < 0) {
				offset = str->size + offset;
			}

			while (offset > str->size) {
				offset -= str->size;
			}

			size = size < 0 ? str->size - offset :
				(size > str->size - offset ? str->size - offset : size);

			/* copy substring to new string. */
			xclie_strcat(new_str, str->memory + offset, size);
		}
	}

	return new_str;
}

/* right trim (store result on self instance). */
X_INLINE static xclie_string* xclie_rtrim_self(xclie_string* str, const char* characters) {
	int32_t cursor, exit_loop = 0;

	if ((str && xclie_gc_is_registered(&str->gc_entry)) &&
		characters && characters[0] != 0)
	{
		while (str->size && !exit_loop) {
			cursor = 0;
			exit_loop = 1;

			while (str->size && characters[cursor++]) {
				if (str->memory[str->size - 1]
					== characters[cursor  - 1])
				{
					str->memory[--str->size] = 0;
					exit_loop = 0;
					break;
				}
			}
		}
	}

	return str;
}

/* right trim (store result on new instance). */
X_INLINE static xclie_string* xclie_rtrim(xclie_string* str, const char* characters) {
	return xclie_rtrim_self(str ? xclie_substr(str, 0, str->size) : nullptr, characters);
}

X_INLINE static xclie_string* sclie_ltrim_self(xclie_string* str, const char* characters) {
	int32_t cursor;
	char* reader;

	if (str && xclie_gc_is_registered(&str->gc_entry) &&
		characters && characters[0] != 0)
	{
		reader = str->memory;

		while (str->size) {
			cursor = 0;

			while (str->size && reader[0] &&
				characters[cursor++])
			{
				if (reader[0] == characters[cursor - 1]) {
					reader++;
					break;
				}
			}

			/* end of string or no more matches.*/
			if (!reader[0] || reader[0] != characters[cursor - 1]) {
				break;
			}
		}

		if (!reader[0]) {
			/* kill string memory. */
			xclie_string_resize(str, 0);
		}

		/* no more matches. */
		else if ((size_t) reader > (size_t)(str->memory)) {
			memmove(str->memory, reader, str->size -
				((size_t)reader - (size_t)str->memory));

			str->size -= (int32_t)((size_t)reader - (size_t)str->memory);
			str->memory[str->size] = 0;
		}
	}

	return str;
}

X_INLINE static xclie_string* sclie_ltrim(xclie_string* str, const char* characters) {
	return sclie_ltrim_self(str ? xclie_substr(str, 0, str->size) : nullptr, characters);
}

#endif
