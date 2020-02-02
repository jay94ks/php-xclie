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
#define __XCLIE_H__

#ifdef __INTELLISENSE__
typedef unsigned long long size_t;
typedef long long ssize_t;

#define PHP_WIN32
#define ZEND_WIN32
#endif

#ifdef __XCLIE__
#	define XCLIE_INTERNAL	1
#	include <main/php.h>
#	include <main/SAPI.h>
#	include <main/php_main.h>
#	include <main/php_variables.h>
#	include <main/php_ini.h>
#	include <zend_ini.h>

#	include "ext/standard/php_standard.h"

#	ifndef PHP_WIN32
#		define XCLIE_API SAPI_API
#	else
#		define XCLIE_API
#	endif

#	ifdef PHP_WIN32
#		include <io.h>
#		include <fcntl.h>
#		include <direct.h>
#	endif

#	ifdef ZTS
ZEND_TSRMLS_CACHE_EXTERN()
#	endif
#else // ! defined (__XCLIE__)
#	define XCLIE_INTERNAL	0
#	ifndef __USE_XCLIE__
#	define __USE_XCLIE__
#	if defined(WIN32) || defined(WIN64) || \
	   defined(_WIN32) || defined(_WIN64)
#		ifdef __cplusplus
#			define XCLIE_API	extern
#		else
#			define XCLIE_API	extern "C" extern
#		endif
#	else
#		define XCLIE_API
#	endif
#	endif

#	include <stddef.h>
#	include <stdint.h>
#	include <malloc.h>
#endif // __XCLIE__

/* this is for my habit... */
#ifndef __cplusplus
#define nullptr		NULL
#endif

#ifndef _MSC_VER
#define X_INLINE	inline
#else
#define X_INLINE	__forceinline
#endif

#define XCLIE_STRUCT(name, ...)	typedef __VA_ARGS__ name;
#ifdef __cplusplus
#define XCLIE_CXX_FWD(name, cxx_name) \
	namespace xclie { using cxx_name = ::name; }

#define XCLIE_CXX_ALIAS(...) \
	namespace xclie { X_INLINE __VA_ARGS__ }
#else
#define XCLIE_CXX_FWD(name, cxx_name)
#define XCLIE_CXX_ALIAS(...)
#endif

/* version information */
XCLIE_STRUCT(xclie_version, struct {
	int32_t x, y;
});

/* C++ type forward. */
XCLIE_CXX_FWD(xclie_version, version);

enum {
	/* major version number. */
	XCVER_MAJOR	= 1,

	/* minor version number. */
	XCVER_MINOR = 0
};

/* get xclie header version. */
X_INLINE static void xclie_get_header_version(xclie_version* buf) {
	if (buf) { buf->x = XCVER_MAJOR; buf->y = XCVER_MINOR; }
}
XCLIE_CXX_ALIAS(void get_header_version(xclie_version* buf) {
	xclie_get_header_version(buf);
});

/* get xclie compiled version. */
XCLIE_API void xclie_get_version(xclie_version* buf);
XCLIE_CXX_ALIAS(void get_version(xclie_version* buf) {
	xclie_get_version(buf);
});

/* garbage collector. */
#include "bits/xclie_gc.h"

/* status codes. */
#include "bits/xclie_status.h"

/* register an object to gc. */
X_INLINE static void xclie_gc_register(xclie_gc_root* gc_root,
	xclie_gc_entry* entry, void(*destroy)(xclie_gc_entry* entry))
{
	if (entry && !entry->owner) {
		if (!gc_root) {
			gc_root = &(xclie_get_status()->gc_root);
		}

		entry->owner = gc_root;
		entry->next = entry->prev = 0;
		entry->destroy = destroy;

		if (!gc_root->last && gc_root->first) {
			gc_root->last = gc_root->first;
		}

		if (gc_root->last) {
			gc_root->last->next = entry;
			entry->prev = gc_root->last;
		}

		else {
			gc_root->first = entry;
		}

		gc_root->last = entry;
	}
}

/* gc_alloc, gc_free. */
XCLIE_API void* xclie_gc_alloc_gc(xclie_gc_root* gc_root, int32_t size);
X_INLINE static void* xclie_gc_alloc(int32_t size) {
	return xclie_gc_alloc_gc(nullptr, size);
}

/* determines the block was allocated using `gc_alloc` or not. */
XCLIE_API int32_t xclie_is_gc_alloc(void* block);

/* gc_free. if signature missing or nullptr, this returns 0. otherwise, returns 1. */
XCLIE_API int32_t xclie_gc_free(void* block);

/* prototype for gc-deinitializer. */
typedef void(*xclie_gc_deinit)(void*);

/* to set de-initializer for the block. */
XCLIE_API xclie_gc_deinit xclie_gc_set_deinit(void* block, xclie_gc_deinit);

/* exit codes. */
#include "bits/xclie_exit_code.h"

/* xclie string library. */
#include "xclie_string.h"

/* xclie exec argument. */
#include "xclie_args.h"

/* execute XCLIE. */
XCLIE_API static int32_t xclie_run(const xclie_args* args);

/* cleanup XCLIE. */
XCLIE_API static int32_t xclie_cleanup();

// --------------------------------------------------------------------
#ifdef __XCLIE_C__
#define XCLIE_INI_HTML_ERRORS			"html_errors"
#define XCLIE_INI_REGISTER_ARGC_ARGV	"register_argc_argv"
#define XCLIE_INI_IMPLICIT_FLUSH		"implicit_flush"
#define XCLIE_INI_OUTPUT_BUFFERING		"output_buffering"
#define XCLIE_INI_MAX_EXECUTION_TIME	"max_execution_time"
#define XCLIE_INI_MAX_INPUT_TIME		"max_input_time"

#define XCLIE_DEFAULT_INIT_ENTRIES \
	XCLIE_INI_HTML_ERRORS "=0\n" XCLIE_INI_REGISTER_ARGC_ARGV "=1\n" \
	XCLIE_INI_IMPLICIT_FLUSH "=1\n" XCLIE_INI_OUTPUT_BUFFERING "=0\n" \
	XCLIE_INI_MAX_EXECUTION_TIME "=0\n" XCLIE_INI_MAX_INPUT_TIME "=-1\n"

void xclie_init_platform();
void xclie_deinit_platform();

xclie_string* xclie_get_exec_file_path(xclie_gc_root* gc_root);
xclie_string* xclie_get_exec_cwd(xclie_gc_root* gc_root);
void xclie_configure_exec_path();

void xclie_bootstrap();
void xclie_debootstrap();

void xclie_translate_arguments();
void xclie_make_path_to_exec_based(
	xclie_string* in_path, xclie_string* out_path);

void xclie_load_ini_settings();
void xclie_set_constant(const char* name, const char* value);
void xclie_register_std_handles(int32_t no_close);

#ifdef PHP_WIN32
DWORD WINAPI xclie_threaded_main(LPVOID);
#else
void* xclie_threaded_main(void*);
#endif

#define XCLIE_MEMBLOCK_SIGNATURE		((uint32_t)(0xCACAFEFEu))
#endif
#endif
