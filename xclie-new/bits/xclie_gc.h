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

#ifndef __XCLIE_H__
#error	"bits/xclie_status.h can not be standalone!"
#else

struct _xclie_gc_entry;

/*
	xclie gc root.
	xclie-gc is for holding a library internal memory.
*/
XCLIE_STRUCT(xclie_gc_root, struct {
	struct _xclie_gc_entry* first, *last;
});

XCLIE_CXX_FWD(xclie_gc_root, gc_root);

XCLIE_STRUCT(xclie_gc_entry, struct _xclie_gc_entry {
	xclie_gc_root* owner;
	struct _xclie_gc_entry* next, *prev;
	void(*destroy)(struct _xclie_gc_entry* entry);
});

X_INLINE static int32_t xclie_gc_is_registered(const xclie_gc_entry* entry) {
	return entry && entry->owner;
}


/* unregister an object from gc. */
X_INLINE static void xclie_gc_unregister(xclie_gc_entry* entry) {
	xclie_gc_root* gc_root;

	if (entry && entry->owner) {
		gc_root = entry->owner;
		entry->owner = NULL;

		if (entry->next) {
			entry->next->prev = entry->prev;
		}

		if (entry->prev) {
			entry->prev->next = entry->next;
		}

		if (gc_root->first == entry) {
			gc_root->first = entry->next;
		}

		if (gc_root->last == entry) {
			gc_root->last = entry->prev;
		}
	}
}

/* initialize gc_root. */
X_INLINE static void xclie_gc_root_init(xclie_gc_root* gc_root) {
	gc_root->first = gc_root->last = 0;
}

/* deinitialize gc_root. */
X_INLINE static void xclie_gc_root_deinit(xclie_gc_root* gc_root) {
	xclie_gc_entry* cursor = gc_root->last, *temp;

	while (cursor && cursor->next)
		cursor = cursor->next;

	while (cursor) {
		temp = cursor->prev;
		xclie_gc_unregister(cursor);

		cursor->destroy(cursor);
		cursor = temp;
	}
}


#endif
