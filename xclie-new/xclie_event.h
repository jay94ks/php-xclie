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

#ifndef __XCLIE_EVENT_H__
#define __XCLIE_EVENT_H__

#ifndef __XCLIE_H__
#include "xclie.h"
#endif

#if XCLIE_INTERNAL
void xclie_event_init();
void xclie_event_deinit();

PHP_FUNCTION(xclie_event_issue);
PHP_FUNCTION(xclie_event_wait);
#endif

/*
	xclie_event.
	stored strings should be copied somewhere
	at least before PHP script completed.

	and call drop() callback which stored in structure.
*/
XCLIE_STRUCT(xclie_event, struct _xclie_event {
	xclie_string* event_type;
	xclie_string* event_data;

	/* drop this event. */
	void(*drop)(struct _xclie_event*);
});

/* php function has `zif_` prefix for their names. */
XCLIE_API int32_t xclie_event_issue(const char* type, const char* data);
XCLIE_API int32_t xclie_event_wait(xclie_event* out_event, int32_t timeout);

#endif // __XCLIE_EVENT_H__
