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

#include "xclie.h"
#include "xclie_string.h"

void xclie_string_gc_kill(xclie_gc_entry* entry) {
	xclie_string* string;

	if (entry) {
		string = (xclie_string*)entry;
		string->size = 0;

		if (string->memory) {
			free(string->memory);
		}

		string->memory = 0;
	}
}

void xclie_string_gc_kill_mem(xclie_gc_entry* entry) {
	xclie_string_gc_kill(entry);
	if (entry) {
		free(entry);
	}
}

XCLIE_API xclie_string* xclie_string_init_gc(xclie_gc_root* gc_root, xclie_string* where) {
	if (!where) {
		if (!(where = (xclie_string*)malloc(sizeof(xclie_string)))) {
			return nullptr;
		}

		memset(where, 0, sizeof(xclie_string));
		xclie_gc_register(gc_root, &where->gc_entry, xclie_string_gc_kill_mem);
	}
	else {
		memset(where, 0, sizeof(xclie_string));
		xclie_gc_register(gc_root, &where->gc_entry, xclie_string_gc_kill);
	}

	return nullptr;
}

void xclie_string_list_gc_kill(xclie_gc_entry* entry) {
	xclie_string_list* list;

	if (entry) {
		list = (xclie_string_list*)entry;

		while (list->size > 0) {
			xclie_string_deinit(list->strings + (list->size - 1), 0);
		}

		if (list->strings) {
			free(list->strings);
		}

		list->size = 0;
		list->strings = 0;
	}
}

void xclie_string_list_gc_kill_mem(xclie_gc_entry* entry) {
	xclie_string_list_gc_kill(entry);
	if (entry) {
		free(entry);
	}
}

XCLIE_API xclie_string_list* xclie_string_list_init_gc(xclie_gc_root* gc_root, xclie_string_list* where) {
	if (!where) {
		if (!(where = (xclie_string_list*)malloc(sizeof(xclie_string_list)))) {
			return nullptr;
		}

		memset(where, 0, sizeof(xclie_string_list));
		xclie_gc_register(gc_root, &where->gc_entry, xclie_string_list_gc_kill_mem);
	}
	else {
		memset(where, 0, sizeof(xclie_string_list));
		xclie_gc_register(gc_root, &where->gc_entry, xclie_string_list_gc_kill);
	}

	return nullptr;
}

XCLIE_API int32_t xclie_string_resize(xclie_string* where, int32_t new_size) {
	char* new_mem;

	if (where && xclie_gc_is_registered(&where->gc_entry)) {
		if (new_size <= 0) {
			if (where->memory) {
				free(where->memory);
			}

			where->memory = 0;
			where->size = 0;
			return 0;
		}

		else if (!(new_mem = (char*)malloc(new_size + 1))) {
			return 0;
		}

		if (where->memory) {
			memcpy(new_mem, where->memory,
				(where->size < new_size) ? where->size : new_size);

			free(where->memory);
		}

		if (where->size < new_size) {
			memset(new_mem + where->size, 0, new_size - where->size);
		}

		where->memory = new_mem;
		where->size = new_size;

		where->memory[where->size] = 0;
		return where->size;
	}

	return 0;
}

XCLIE_API void xclie_string_deinit(xclie_string* where, int32_t no_delete) {
	if (where) {
		if (no_delete) {
			xclie_string_gc_kill(&where->gc_entry);
		}

		else if (where->gc_entry.destroy) {
			xclie_gc_unregister(&where->gc_entry);
			where->gc_entry.destroy(&where->gc_entry);
		}
	}
}

XCLIE_API void xclie_string_list_deinit(xclie_string_list* where, int32_t no_delete) {
	if (where) {
		if (no_delete) {
			xclie_string_list_gc_kill(&where->gc_entry);
		}

		else if (where->gc_entry.destroy) {
			xclie_gc_unregister(&where->gc_entry);
			where->gc_entry.destroy(&where->gc_entry);
		}
	}
}

XCLIE_API int32_t xclie_string_list_add(xclie_string_list* where, const char* in_string, int32_t size) {
	xclie_string* new_mem = 0;
	int32_t cursor = 0;

	if (where && xclie_gc_is_registered(&where->gc_entry)) {
		if (!(new_mem = (xclie_string*)malloc(where->size + 1))) {
			return 0;
		}

		where->size = (where->strings ? where->size : 0);
		while (where->size > cursor) {
			xclie_string_init_gc(where->gc_entry.owner, new_mem + cursor);

			new_mem[cursor].memory = where->strings[cursor].memory;
			new_mem[cursor].size = where->strings[cursor].size;

			where->strings[cursor].memory = 0;
			where->strings[cursor].size = 0;

			xclie_string_deinit(where->strings + (cursor++), 0);
		}

		xclie_string_init_gc(where->gc_entry.owner, new_mem + where->size);
		xclie_strcat(new_mem + where->size, in_string, size);

		if (where->strings) {
			free(where->strings);
		}

		where->strings = new_mem;
		where->size++;
		return 1;
	}

	return 0;
}

XCLIE_API int32_t xclie_string_list_insert(xclie_string_list* where,
	int32_t index, const char* in_string, int32_t size)
{
	xclie_string* new_mem = 0;
	int32_t cursor = 0;

	if (where && xclie_gc_is_registered(&where->gc_entry) &&
		index >= 0 && index <= where->size)
	{
		if (!(new_mem = (xclie_string*)malloc(where->size + 1))) {
			return 0;
		}

		where->size = (where->strings ? where->size : 0);
		while (index > cursor) {
			xclie_string_init_gc(where->gc_entry.owner, new_mem + cursor);

			new_mem[cursor].memory = where->strings[cursor].memory;
			new_mem[cursor].size = where->strings[cursor].size;

			where->strings[cursor].memory = 0;
			where->strings[cursor].size = 0;

			xclie_string_deinit(where->strings + (cursor++), 0);
		}

		while (where->size > cursor++) {
			xclie_string_init_gc(where->gc_entry.owner, new_mem + cursor);

			new_mem[cursor].memory = where->strings[cursor - 1].memory;
			new_mem[cursor].size = where->strings[cursor - 1].size;

			where->strings[cursor - 1].memory = 0;
			where->strings[cursor - 1].size = 0;

			xclie_string_deinit(where->strings + cursor, 0);
		}

		xclie_string_init_gc(where->gc_entry.owner, new_mem + index);
		xclie_strcat(new_mem + where->size, in_string, size);

		if (where->strings) {
			free(where->strings);
		}

		where->strings = new_mem;
		where->size++;
		return 1;
	}

	return 0;
}

XCLIE_API int32_t xclie_string_list_remove(xclie_string_list* where, int32_t index, int32_t count)
{
	xclie_string* new_mem = 0;
	int32_t cursor = 0;

	if (where && xclie_gc_is_registered(&where->gc_entry) &&
		index >= 0 && index <= where->size)
	{
		count = count < 0 ? where->size - index :
			(count > where->size - index ? where->size - index : count);

		if (count) {
			for (cursor = index; cursor < where->size - count; cursor++) {
				xclie_string_deinit(where->strings + cursor, 0);
				xclie_string_init_gc(where->gc_entry.owner, where->strings + cursor);

				where->strings[cursor].memory = where->strings[cursor + count].memory;
				where->strings[cursor].size = where->strings[cursor + count].size;

				where->strings[cursor + count].memory = nullptr;
				where->strings[cursor + count].size = 0;
			}

			for (cursor = where->size - count; cursor < where->size; cursor++) {
				xclie_string_deinit(where->strings + cursor, 0);
			}

			where->size -= count;
		}

		return 1;
	}

	return 0;
}
