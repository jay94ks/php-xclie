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

#define __XCLIE_C__
#define __XCLIE_C_EXTERN__

#include "xclie.h"
#include "xclie_event.h"

XCLIE_STRUCT(xclie_event_item,
	struct _xclie_event_item
{
	struct _xclie_event_item* next;
	int32_t is_issue;

	xclie_string* event_type;
	xclie_string* event_data;
});

#ifdef PHP_WIN32
CRITICAL_SECTION event_cs;
HANDLE event_hdl_native_act;
HANDLE event_hdl_php_act;
#else
pthread_mutex_t event_cs;
pthread_cond_t event_hdl_native_act;
pthread_cond_t event_hdl_php_act;
#endif

int32_t event_cs_ready = 0;
xclie_event_item* event_first, *event_last;
xclie_gc_root event_gc;

enum {
	XCEVE_FROM_PHP,
	XCEVE_FROM_NATIVE
};

void xclie_event_init() {
	if (!event_cs_ready) {
		event_cs_ready = 1;

		event_first = event_last = nullptr;
		xclie_gc_root_init(&event_gc);

#ifdef PHP_WIN32
		InitializeCriticalSection(&event_cs);
		event_hdl_native_act = CreateEventA(nullptr, FALSE, FALSE, nullptr);
		event_hdl_php_act = CreateEventA(nullptr, FALSE, FALSE, nullptr);
#else
		pthread_mutex_init(&event_cs, nullptr);
		pthread_cond_init(&event_hdl_native_act, nullptr);
		pthread_cond_init(&event_hdl_php_act, nullptr);
#endif
	}
}

void xclie_event_deinit() {
	if (event_cs_ready) {
		event_cs_ready = 0;

#ifdef PHP_WIN32
		CloseHandle(event_hdl_native_act);
		CloseHandle(event_hdl_php_act);
		DeleteCriticalSection(&event_cs);
#else
		pthread_cond_destroy(&event_hdl_php_act);
		pthread_cond_destroy(&event_hdl_native_act);
		pthread_mutex_destroy(&event_cs);
#endif

		event_first = event_last = nullptr;
		xclie_gc_root_deinit(&event_gc);
	}
}

static int32_t xclie_event_issue_r(
	int32_t is_issue, const char* type, const char* data,
	int32_t type_len, int32_t data_len)
{
	if (!g_global_status->is_threaded ||
		(!type && !data))
	{
		return 0;
	}

#ifdef PHP_WIN32
	EnterCriticalSection(&event_cs);
#else
	pthread_mutex_lock(&event_cs);
#endif

	if (!event_last) {
		event_last = event_first = (xclie_event_item*)
			xclie_gc_alloc_gc(&event_gc, sizeof(xclie_event_item));
	}
	else {
		event_last->next = (xclie_event_item*)
			xclie_gc_alloc_gc(&event_gc, sizeof(xclie_event_item));

		event_last = event_last->next;
	}

	memset(event_last, 0, sizeof(xclie_event_item));
	event_last->is_issue = is_issue;

	xclie_strcat(event_last->event_data =
		xclie_string_init_gc(&event_gc, nullptr),
		data, data_len);

	xclie_strcat(event_last->event_type =
		xclie_string_init_gc(&event_gc, nullptr),
		type, type_len);

#ifdef PHP_WIN32
	SetEvent(is_issue == XCEVE_FROM_NATIVE ?
		event_hdl_php_act : event_hdl_native_act);

	LeaveCriticalSection(&event_cs);
#else
	pthread_cond_signal(is_issue == XCEVE_FROM_NATIVE ?
		&event_hdl_php_act : &event_hdl_native_act);
	pthread_mutex_unlock(&event_cs);
#endif

	return 1;
}

void xclie_event_item_drop(xclie_event* event) {
	xclie_string_deinit(event->event_type, 0);
	xclie_string_deinit(event->event_data, 0);

	memset(event, 0, sizeof(xclie_event));
}

int32_t xclie_event_wait_r(int32_t is_issue, xclie_event* out_event, int32_t timeout) {
	int32_t retVal = 0;

	xclie_event_item* cursor;
	xclie_event_item** dbl_cursor;

#ifndef PHP_WIN32
	struct timespec tv;
	int32_t temp = 0;
#endif

	clock_t last_clock = clock();
	int32_t time_slice = 0;

	if (!g_global_status->is_threaded) {
		return 0;
	}

	/*
		Before entering into blocked-wait,
		check event queue once.
	*/

	while (1) {
#ifdef PHP_WIN32
		EnterCriticalSection(&event_cs);
#else
		pthread_mutex_lock(&event_cs);
#endif

		cursor = event_first;
		dbl_cursor = &event_first;

		while (cursor) {

			/* skip events which unmatched. */
			if (cursor->is_issue != is_issue) {
				dbl_cursor = &cursor->next;
				cursor = cursor->next;
				continue;
			}

			break;
		}

		/* there is an event! */
		if (cursor) {
			retVal = 1;

			/* no remove if no pointer set. */
			if (out_event) {
				/* fill out out-event.*/
				out_event->event_data = cursor->event_data;
				out_event->event_type = cursor->event_type;
				out_event->drop = xclie_event_item_drop;

				cursor->event_data = nullptr;
				cursor->event_type = nullptr;

				*dbl_cursor = cursor->next;
				if (cursor == event_last) {
					event_last = *dbl_cursor;
				}

				xclie_gc_free(cursor);
			}
		}

#ifdef PHP_WIN32
		LeaveCriticalSection(&event_cs);
#endif

		if (!retVal) {
			if (timeout < 0) {
#ifdef PHP_WIN32
				WaitForSingleObject(is_issue == XCEVE_FROM_NATIVE ?
					event_hdl_native_act : event_hdl_php_act, INFINITE);
#else
				if (is_issue == XCEVE_FROM_NATIVE) {
					pthread_cond_wait(&event_hdl_native_act, &event_cs);
				}
				else {
					pthread_cond_wait(&event_hdl_php_act, &event_cs);
				}

				pthread_mutex_unlock(&event_cs);
#endif
			}
			else {
				time_slice = timeout < 1000 ? timeout : 1000;

#ifdef PHP_WIN32
				if (WaitForSingleObject(is_issue == XCEVE_FROM_NATIVE ?
					event_hdl_native_act : event_hdl_php_act, time_slice)
					!= WAIT_OBJECT_0)
				{
					if (!time_slice) {
						break;
					}
				}
#else
				tv.tv_sec = time_slice / 1000;
				tv.tv_nsec = (time_slice % 1000) * 1000;

				if (is_issue == XCEVE_FROM_NATIVE) {
					temp = pthread_cond_timedwait(&event_hdl_native_act, &event_cs, &tv);
				}
				else {
					temp = pthread_cond_timedwait(&event_hdl_php_act, &event_cs, &tv);
				}

				if (temp && !time_slice) {
					break;
				}

				pthread_mutex_unlock(&event_cs);
#endif

				time_slice = (int32_t)(
					((clock() - last_clock) / (CLOCKS_PER_SEC * 1.0f))
					* 1000.0f);

				if ((timeout = timeout - time_slice) < 0) {
					timeout = 0;
				}
			}

			continue;
		}

#ifndef PHP_WIN32
		pthread_mutex_unlock(&event_cs);
#endif
		break;
	}

	return retVal;
}

XCLIE_API int32_t xclie_event_issue(const char* type, const char* data) {
	return xclie_event_issue_r(XCEVE_FROM_NATIVE, type, data, -1, -1);
}

XCLIE_API int32_t xclie_event_wait(xclie_event* out_event, int32_t timeout) {
	return xclie_event_wait_r(XCEVE_FROM_PHP, out_event, timeout);
}

PHP_FUNCTION(xclie_event_issue)
{
	char* type = 0, *data = 0;
	int32_t type_len = 0, data_len = 0;

	if (!g_global_status->is_threaded) {
		RETURN_BOOL(0);
	}

	/*
		usages:
		1. xclie_event_issue(event_type, event_data)
		2. xclie_event_issue(event_data)
	*/
	switch (ZEND_NUM_ARGS()) {
	case 1:
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
			"s", &data, &data_len) == FAILURE)
		{
			RETURN_BOOL(0);
		}
		break;

	case 2:
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
			"ss", &type, &type_len, &data, &data_len) == FAILURE)
		{
			RETURN_BOOL(0);
		}
		break;

	case 0:
	default:
		RETURN_BOOL(0);
	}

	RETURN_BOOL(xclie_event_issue_r(XCEVE_FROM_PHP, type, data, type_len, data_len));
}

PHP_FUNCTION(xclie_event_wait) {
	long timeout = 0;
	xclie_event out_event;

	if (!g_global_status->is_threaded) {
		RETURN_BOOL(0);
	}

	/*
		usages:
		1. xclie_event_wait(timeout)					until timeout (0 for polling)
		2. xclie_event_wait()							infinite wait

		return value:
			array ( type => event-type, data => event-data )
	*/

	switch (ZEND_NUM_ARGS()) {
	case 0:
		timeout = -1;
		break;

	case 1:
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
			"l", &timeout) == FAILURE)
		{
			RETURN_BOOL(0);
		}
		break;
	}

	if (xclie_event_wait_r(XCEVE_FROM_NATIVE, &out_event, timeout)) {
		array_init(return_value);

		if (out_event.event_type->memory) {
			add_assoc_string_ex(return_value, "type", 4, out_event.event_type->memory);
		}

		if (out_event.event_data->memory) {
			add_assoc_string_ex(return_value, "data", 4, out_event.event_data->memory);
		}

		out_event.drop(&out_event);
		return;
	}

	RETURN_BOOL(0);
}

