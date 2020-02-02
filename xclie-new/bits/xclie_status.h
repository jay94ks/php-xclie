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

enum {
	/* Did nothing. */
	XCSTAT_NONE = 0,

	/* Status object initialized. */
	XCSTAT_INIT	= 1,

	/* Bootstraping PHP engine. */
	XCSTAT_BOOTSTRAP = 2,

	/* Loading PHP modules. */
	XCSTAT_MODULE_LOAD = 4,

	/* Starting interpreter. */
	XCSTAT_STARTUP = 8,

	/* XCLIE ready to work. */
	XCSTAT_READY = (1 | 2 | 4 | 8),

	/* Not completed to protect flow. */
	XCSTAT_NOT_COMPLETED = 16
};

struct _xclie_args;

/*
	critical section and pthread mutex
	is only for guarding `background`ed thread during startup.
*/
#ifdef PHP_WIN32
#define XCLIE_STATUS_THRDS	\
	CRITICAL_SECTION cs;	\
	void* thread_handle;	\
	void* event_handle

#else
#define XCLIE_STATUS_THRDS	\
	pthread_mutex_t cs;		\
	pthread_t thread_handle;\
	signal_t sigpipe_bkp

#endif

#if XCLIE_INTERNAL
/* opacity pointers. */
#define XCLIE_STATUS_OPTY				\
	sapi_module_struct* module_data;	\
	zend_file_handle* main_program;		\
	zend_llist* global_vars
#else
/* opacity pointers. */
#define XCLIE_STATUS_OPTY				\
	void* module_data;	\
	void* main_program;		\
	void* global_vars
#endif

/*
	xclie status object (global)
*/
XCLIE_STRUCT(xclie_status, struct {
	xclie_gc_root gc_root;
	int32_t status_code;				/* refer bits/xclie_status_codes.h file. */

	struct _xclie_args* exec_args;		/* arguments that supplied by xclie_run() call. */
	struct _xclie_args* running_args;	/* arguments that modified by xclie internal impls. */

	int8_t platform_init;				/* WIN32: already std{io, out, err} set binary or not.
										   UNIX~: SIGPIPE is already set SIG_IGN or not. */

	struct {
		/* set PHP constant. (null if available time has gone) */
		void(*set_constant)(const char* name, const char* value);

	} exec_env;

	int8_t is_threaded;

	char** argv_translated;
	int32_t argc_translated;

	XCLIE_STATUS_THRDS;
	XCLIE_STATUS_OPTY;
});

XCLIE_CXX_FWD(xclie_status, status);

/* get xclie status object. */
XCLIE_API xclie_status* xclie_get_status(void);

/* to check status. */
#define xclie_test_status(flag) (((xclie_get_status()->status_code) & (flag)) == (flag))

#if XCLIE_INTERNAL
/* initialize xclie_status. */
void xclie_status_init();

/* de-initialize xclie_status. */
void xclie_status_deinit();
#endif

#ifdef __XCLIE_C__
#ifdef __XCLIE_C_EXTERN__
extern xclie_status* g_global_status;
#endif

/* These are only for `xclie_args.c` and required files. */
#define g_xclie_status_code		g_global_status->status_code
#define g_xclie_exec_args		g_global_status->exec_args
#define g_xclie_running_args	g_global_status->running_args
#define g_xclie_module_data		g_global_status->module_data
#endif
#endif
