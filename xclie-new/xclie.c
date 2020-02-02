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

#include "xclie.h"
#include "xclie_args.h"
#include "xclie_event.h"

// ----------------------------------------------------------------

/*
	The reason why this isn't `extern`ed is,
	for `as_backend` option.

	this is used for accessing the global status and,
	checking xclie is already running or not.
*/
xclie_status* g_global_status = 0;

XCLIE_STRUCT(xclie_gc_memblock, struct {
	xclie_gc_entry gc_entry;
	xclie_gc_deinit deinit;
	uint32_t signature;
});

xclie_status g_xclie_status = { 0, };

// ----------------------------------------------------------------

sapi_module_struct php_xclie_factory_default = { 0, }; // struct to store backup copy of.

// ----------------------------------------------------------------
/* php module interface. */

int xclie_php_startup(sapi_module_struct *sapi_module);
int xclie_php_deactivate(void);

size_t xclie_php_ub_write(const char *str, size_t str_length);
void xclie_php_flush(void *server_context);

static int xclie_php_header_handler(sapi_header_struct* h, sapi_header_op_enum op, sapi_headers_struct *s) { return 0; }
static void xclie_php_send_header(sapi_header_struct* sapi_header, void *server_context) { }
static int xclie_php_send_headers(sapi_headers_struct* sapi_headers) { return SAPI_HEADER_SENT_SUCCESSFULLY; }
static char* xclie_php_read_cookies(void) { return NULL; }

static void xclie_php_register_variables(zval* tva) { php_import_environment_variables(tva); }
static void xclie_php_log_message(char *message, int syslog_type_int) { fprintf(stderr, "%s\n", message); }

// ----------------------------------------------------------------
/* Module data block. */
XCLIE_API extern sapi_module_struct php_xclie_module = {
	"xclie",                       /* name */
	"PHP eXtendable CLI Embeded",  /* pretty name */

	xclie_php_startup,              /* startup */
	php_module_shutdown_wrapper,   /* shutdown */

	NULL,                          /* activate */
	xclie_php_deactivate,           /* deactivate */

	xclie_php_ub_write,             /* unbuffered write */
	xclie_php_flush,                /* flush */
	NULL,                          /* get uid */
	NULL,                          /* getenv */

	php_error,                     /* error handler */

	xclie_php_header_handler,      /* header handler */
	xclie_php_send_headers,        /* send headers handler */
	xclie_php_send_header,         /* send header handler */

	NULL,                          /* read POST data */
	xclie_php_read_cookies,         /* read Cookies */

	xclie_php_register_variables,   /* register server variables */
	xclie_php_log_message,          /* Log message */
	NULL,							/* Get request time */
	NULL,							/* Child terminate */

	STANDARD_SAPI_MODULE_PROPERTIES
};
/* }}} */ // <-- is this neccessary? :(

/* {{{ arginfo ext/standard/dl.c */
ZEND_BEGIN_ARG_INFO(arginfo_dl, 0)
	ZEND_ARG_INFO(0, extension_filename)
ZEND_END_ARG_INFO()
/* }}} */

PHP_FUNCTION(xclie_event_active) {
	RETURN_BOOL(g_global_status->is_threaded);
}

static const zend_function_entry additional_functions[] = {
	ZEND_FE(xclie_event_active, NULL)
	ZEND_FE(xclie_event_issue, NULL)
	ZEND_FE(xclie_event_wait, NULL)
	ZEND_FE(dl, arginfo_dl) { NULL, NULL, NULL }
};

const char* g_xclie_ini_disallowed[] = {
	XCLIE_INI_HTML_ERRORS,
	XCLIE_INI_REGISTER_ARGC_ARGV,
	XCLIE_INI_IMPLICIT_FLUSH,
	XCLIE_INI_OUTPUT_BUFFERING,
	XCLIE_INI_MAX_EXECUTION_TIME,
	XCLIE_INI_MAX_INPUT_TIME,

	NULL
};

// ----------------------------------------------------------------

XCLIE_API void xclie_get_version(xclie_version* buf) {
	xclie_get_header_version(buf);
}

XCLIE_API xclie_status* xclie_get_status(void) {
	if (!g_xclie_status.status_code) {
		xclie_status_init();
	}

	return &g_xclie_status;
}

void xclie_status_init() {
	if (!g_xclie_status.status_code) {
		g_xclie_status.status_code = XCSTAT_INIT;
		xclie_gc_root_init(&g_xclie_status.gc_root);
	}
}

void xclie_status_deinit() {
	if (g_xclie_status.status_code) {
		xclie_gc_root_deinit(&g_xclie_status.gc_root);
		memset(&g_xclie_status, 0, sizeof(g_xclie_status));
	}
}

void xclie_gc_alloc_kill(xclie_gc_entry* entry) {
	xclie_gc_memblock* mem_head;

	if (entry) {
		mem_head = (xclie_gc_memblock*)entry;

		if (mem_head->deinit) {
			mem_head->deinit(mem_head + 1);
		}

		free(mem_head);
	}
}

/* gc_alloc, gc_free. */
XCLIE_API void* xclie_gc_alloc_gc(xclie_gc_root* gc_root, int32_t size) {
	xclie_gc_memblock* mem_head;

	if (size > 0 && (mem_head = (xclie_gc_memblock*)malloc(size + sizeof(xclie_gc_memblock)))) {
		memset(mem_head, 0, sizeof(xclie_gc_memblock));
		xclie_gc_register(gc_root, &mem_head->gc_entry, xclie_gc_alloc_kill);
		return mem_head + 1;
	}

	return nullptr;
}

X_INLINE static xclie_gc_memblock* xclie_gc_mbhead(void* block) {
	return ((xclie_gc_memblock*)block - 1);
}

XCLIE_API int32_t xclie_is_gc_alloc(void* block) {
	if (block) {
		return
			xclie_gc_mbhead(block)->signature
			== XCLIE_MEMBLOCK_SIGNATURE;
	}

	return 0;
}

XCLIE_API int32_t xclie_gc_free(void* block) {
	if (xclie_is_gc_alloc(block)) {
		xclie_gc_unregister(&xclie_gc_mbhead(block)->gc_entry);
		xclie_gc_alloc_kill(&xclie_gc_mbhead(block)->gc_entry);
		return 1;
	}

	return 0;
}

XCLIE_API xclie_gc_deinit xclie_gc_set_deinit(void* block, xclie_gc_deinit deinit)
{
	xclie_gc_deinit old;

	if (xclie_is_gc_alloc(block)) {
		old = xclie_gc_mbhead(block)->deinit;
		xclie_gc_mbhead(block)->deinit = deinit;
		return old;
	}

	return nullptr;
}

/* test the path points directory or not. */
static int32_t xclie_test_directory(const char* path) {
	struct stat statbuf;

	if (!path || stat(path, &statbuf) ||
		(statbuf.st_mode & S_IFMT) != S_IFDIR)
	{
		return 0;
	}

	return 1;
}

/* test the path points file or not. */
static int32_t xclie_test_file(const char* path) {
	struct stat statbuf;

	if (!path || stat(path, &statbuf) ||
		(statbuf.st_mode & S_IFMT) != S_IFREG)
	{
		return 0;
	}

	return 1;
}

XCLIE_API int32_t xclie_run(const xclie_args* args) {
	int32_t exitCode = XCLIX_SUCCESS;

	if (!g_global_status) {
		g_global_status = xclie_get_status();
		if (g_global_status->status_code != XCSTAT_INIT) {
			xclie_status_init();
		}

		g_xclie_exec_args = (xclie_args*)args;
		g_xclie_module_data = &php_xclie_module;
		g_xclie_status_code |= XCSTAT_NOT_COMPLETED;

		/* make factory default copy if first run. */
		if (!php_xclie_factory_default.name) {
			memcpy(&php_xclie_factory_default,
				g_xclie_module_data,
				sizeof(php_xclie_factory_default));
		}

		/*
			creates running argument status which is used for saving actual status.
			for de-initializing environment correctly.
		*/
		g_xclie_running_args = xclie_gc_alloc(sizeof(xclie_args));
		memset(g_xclie_running_args, 0, sizeof(xclie_args));
		xclie_args_init(g_xclie_running_args, 0, nullptr);

		/* prepare program environment. */
		g_global_status->global_vars
			= (zend_llist*)xclie_gc_alloc(sizeof(zend_llist));

		memset(g_global_status->global_vars, 0, sizeof(zend_llist));
		g_global_status->main_program = nullptr;

		do {
			/* xclie configure platform and exec-directory. */
			xclie_init_platform();
			xclie_event_init();

			xclie_configure_exec_path();

			/* bootstrap PHP core. */
			xclie_bootstrap();

			g_xclie_module_data
			->additional_functions = additional_functions;

			if (args->callbacks.preinit) {
				args->callbacks.preinit(g_global_status,
					xclie_c_str(&g_xclie_running_args->exec_path_override));

				/* copy next callback to running backup. */
				g_xclie_running_args->callbacks.postinit = args->callbacks.postinit;
			}

			/*
				convert xclie string to plain c string.
				and this will set php's executable_location.
			*/
			xclie_translate_arguments();
			xclie_load_ini_settings();

			if (g_xclie_module_data->startup(
				g_xclie_module_data) == FAILURE)
			{
				exitCode = XCLIX_CANT_STARTUP;
				break;
			}

			/* set the module loaded successfully. */
			g_xclie_status_code |= XCSTAT_MODULE_LOAD;
			zend_llist_init(g_global_status->global_vars, sizeof(char *), NULL, 0);

			/* keep working directory to exec location. */
			SG(options) |= SAPI_OPTION_NO_CHDIR;

			/* fill out arguments. */
			SG(request_info).argc = g_global_status->argc_translated;
			SG(request_info).argv = g_global_status->argv_translated;

			if (g_xclie_running_args->callbacks.postinit) {
				g_xclie_running_args->callbacks.postinit(g_global_status);

				/* copy next callback to running backup. */
				g_xclie_running_args->callbacks.startup = args->callbacks.startup;
			}

			xclie_make_path_to_exec_based(&g_xclie_exec_args->entry_script,
				&g_xclie_running_args->entry_script);

			if (!g_xclie_exec_args->entry_script.size) {
				exitCode = XCLIX_NO_FILE_SPEC;
				break;
			}

			if (!xclie_test_file(xclie_c_str(&g_xclie_running_args->entry_script))) {
				exitCode = XCLIX_NO_SUCH_FILE;
				break;
			}
			else {
				g_global_status->main_program
					= (zend_file_handle*)xclie_gc_alloc(sizeof(zend_file_handle));

				memset(g_global_status->main_program, 0, sizeof(zend_file_handle));

				SG(request_info).path_translated
					= xclie_c_str(&g_xclie_running_args->entry_script);

				zend_stream_init_filename(
					g_global_status->main_program,
					xclie_c_str(&g_xclie_running_args->entry_script));
			}

			if (!g_global_status->main_program) {
				exitCode = XCLIX_NO_PERMISSION;
				break;
			}

			if (php_request_startup() == FAILURE) {
				exitCode = XCLIX_CANT_STARTUP;
				break;
			}

			/* set the module loaded successfully. */
			g_xclie_status_code |= XCSTAT_STARTUP;

			/* set no header being. */
			SG(headers_sent) = 1;
			SG(request_info).no_headers = 1;

			g_global_status->exec_env.set_constant = xclie_set_constant;
			if (g_xclie_running_args->callbacks.startup) {
				g_xclie_running_args->callbacks.startup(g_global_status);

				/* copy next callback to running backup. */
				g_xclie_running_args->callbacks.shutdown = args->callbacks.shutdown;
			}

			/* set all available flags on here. */
			g_xclie_status_code |= XCSTAT_READY;

			/* set constants. */
			xclie_set_constant("__EXEC_CWD__",
				xclie_c_str(xclie_get_exec_cwd(nullptr)));

			xclie_set_constant("__EXEC_DIR__",
				g_xclie_running_args->exec_path_override.memory);

			xclie_set_constant("__EXEC_FILE__",
				xclie_c_str(xclie_get_exec_file_path(nullptr)));

			/* register standard streams. */
			if (!args->no_std_handles) {
				xclie_register_std_handles(
					args->no_close_std_handles
				);
			}

			g_global_status->exec_env.set_constant = nullptr;

			/* load script in here. */
			if (php_fopen_primary_script(g_global_status->main_program) == FAILURE) {
				if (errno == EACCES) {
					exitCode = XCLIX_NO_PERMISSION;
				}
				else {
					exitCode = XCLIX_NO_SUCH_FILE;
				}

				if (SG(request_info).path_translated) {
					SG(request_info).path_translated = nullptr;
				}

				break;
			}

			// wanna run the php on background thread.
			if (args->as_backend) {
				g_global_status->is_threaded = 1;
				// CS and mutex is for guarding thread
				// before completion of this function.

#ifdef PHP_WIN32
				EnterCriticalSection(&g_global_status->cs);
				g_global_status->event_handle = CreateEventA(nullptr, TRUE, FALSE, nullptr);

				g_global_status->thread_handle
					= CreateThread(nullptr, 1024 * 1024 * 2, xclie_threaded_main, nullptr, 0, 0);

				if (!g_global_status->thread_handle) {
					exitCode = XCLIX_CANT_STARTUP;
				}
#else
				pthread_mutex_lock(&g_global_status->cs);
				if (pthread_create(&g_global_status->thread_handle,
					nullptr, xclie_threaded_main, nullptr))
				{
					exitCode = XCLIX_CANT_STARTUP;
				}
#endif
#ifdef PHP_WIN32
				LeaveCriticalSection(&g_global_status->cs);
#else
				pthread_mutex_unlock(&g_global_status->cs);
#endif
			}
			else { php_execute_script(g_global_status->main_program); }
		} while (0);

		/* unset protection flag for guarding `xclie_cleanup` at callbacks. */
		g_xclie_status_code &= ~XCSTAT_NOT_COMPLETED;
		if (exitCode == XCLIX_SUCCESS) {
			xclie_cleanup();
		}

		return exitCode;
	}

	/* already running instance is being. */
	return XCLIX_ALREADY;
}

XCLIE_API int32_t xclie_cleanup()
{
	if (g_global_status &&
		!xclie_test_status(XCSTAT_NOT_COMPLETED))
	{
		if (g_global_status->is_threaded) {
			g_global_status->is_threaded = 0;

#ifdef PHP_WIN32
			/* for avoiding bugs, wait termination through event object. */
			WaitForSingleObject(g_global_status->event_handle, INFINITE);

			/* clean all handles. */
			CloseHandle(g_global_status->event_handle);
			CloseHandle(g_global_status->thread_handle);
#else
			pthread_join(g_global_status->thread_handle);
			pthread_detach(g_global_status->thread_handle);
			g_global_status->thread_handle = 0;
#endif
		}

		/* cleanup module. */
		if (xclie_test_status(XCSTAT_STARTUP)) {
			g_xclie_status_code &= ~XCSTAT_STARTUP;

			if (g_xclie_running_args->callbacks.shutdown) {
				g_xclie_running_args->callbacks.shutdown(g_global_status);
				g_xclie_running_args->callbacks.cleanup = g_xclie_exec_args->callbacks.cleanup;
			}

			php_request_shutdown((void *)0);
		}

		/* kill global variables. */
		zend_llist_destroy(g_global_status->global_vars);
		xclie_gc_free(g_global_status->global_vars);

		/* cleanup module. */
		if (xclie_test_status(XCSTAT_MODULE_LOAD)) {
			g_xclie_status_code &= ~XCSTAT_MODULE_LOAD;
			php_module_shutdown();
		}

		if (g_xclie_running_args->callbacks.cleanup) {
			g_xclie_running_args->callbacks.cleanup(g_global_status);
		}

		/* debootstrap PHP core. */
		xclie_debootstrap();

		xclie_event_deinit();
		xclie_deinit_platform();

		/* deinit status. */
		xclie_status_deinit();

		/* restore factory default. */
		memcpy(g_xclie_module_data,
			&php_xclie_factory_default,
			sizeof(php_xclie_factory_default));

		g_xclie_exec_args = nullptr;
		g_global_status = nullptr;

		return XCLIX_SUCCESS;
	}

	/* if not completed yet. */
	if (g_global_status &&
		xclie_test_status(XCSTAT_NOT_COMPLETED))
	{
		return XCLIX_ON_BACKGROUND;
	}

	return XCLIX_ALREADY;
}

void xclie_init_platform() {
	if (!g_global_status->platform_init) {
#if defined(SIGPIPE) && defined(SIG_IGN)
		g_xclie_running_args->no_block_sigpipe = g_xclie_exec_args->no_block_sigpipe;

		if (!g_xclie_exec_args->no_block_sigpipe) {
			g_global_status->sigpipe_bkp = signal(SIGPIPE, SIG_IGN);
			/* ignore SIGPIPE in standalone mode so that sockets created via fsockopen()
			 don't kill PHP if the remote site closes it.  in apache|apxs mode apache
			 does that for us!  thies@thieso.net 20000419 */
		}
#endif
#ifdef PHP_WIN32
		_fmode = _O_BINARY;						/*sets default for file streams to binary */
		setmode(_fileno(stdin), O_BINARY);		/* make the stdio mode be binary */
		setmode(_fileno(stdout), O_BINARY);		/* make the stdio mode be binary */
		setmode(_fileno(stderr), O_BINARY);		/* make the stdio mode be binary */
#endif
	}

#ifdef PHP_WIN32
	InitializeCriticalSection(&g_global_status->cs);
#else
	pthread_mutex_init(&g_global_status->cs, nullptr);
#endif
}

void xclie_deinit_platform() {
	if (g_global_status->platform_init) {
#if defined(SIGPIPE) && defined(SIG_IGN)
		/* revert SIGPIPE handler back. */
		if (!g_xclie_running_args->no_block_sigpipe) {
			signal(SIGPIPE, g_global_status->sigpipe_bkp);
		}

		g_global_status->platform_init = 0;
#endif

		/*
			binary mode can not be reverted correctly.
			so, keep its flag and make no-init run again.
		*/
#ifndef PHP_WIN32
		g_global_status->platform_init = 1;
#endif
	}

#ifdef PHP_WIN32
	DeleteCriticalSection(&g_global_status->cs);
#else
	pthread_mutex_destroy(&g_global_status->cs, nullptr);
#endif
}

xclie_string* xclie_get_exec_file_path(xclie_gc_root* gc_root) {
	char buffer[MAX_PATH + 1];

	memset(buffer, 0, MAX_PATH + 1);

#ifdef PHP_WIN32
	GetModuleFileNameA(NULL, g_xclie_running_args
		->exec_path_override.memory, MAX_PATH);
#else
	readlink("/proc/self/exe", g_xclie_running_args
		->exec_path_override.memory, MAX_PATH);
#endif

	return xclie_strcat(xclie_string_init_gc(gc_root, nullptr), buffer, -1);
}

xclie_string* xclie_get_exec_cwd(xclie_gc_root* gc_root) {
	char buffer[MAX_PATH + 1];

	memset(buffer, 0, MAX_PATH + 1);
	getcwd(buffer, MAX_PATH);

	return xclie_strcat(xclie_string_init_gc(gc_root, nullptr), buffer, -1);
}

void xclie_configure_exec_path() {
#define buffer					(g_xclie_running_args->exec_path_override.memory)
#define buffer_size				(g_xclie_running_args->exec_path_override.size)
#define buffer_resize(size)		xclie_string_resize(&g_xclie_running_args->exec_path_override, size);

	/* if there is exec_path override set, */
	if (g_xclie_exec_args->exec_path_override.size > 0) {
		buffer_resize(0);
		xclie_strcat(&g_xclie_running_args->exec_path_override,
			g_xclie_exec_args->exec_path_override.memory,
			g_xclie_exec_args->exec_path_override.size);

		/* the path isn't pointing directory. */
		if (!xclie_test_directory(g_xclie_exec_args->exec_path_override.memory)) {
			buffer_resize(0);
		}
	}

	if (buffer_size <= 0) {
		buffer_resize(MAX_PATH);

#ifdef PHP_WIN32
		GetModuleFileNameA(NULL, g_xclie_running_args
			->exec_path_override.memory, MAX_PATH);
#else
		readlink("/proc/self/exe", g_xclie_running_args
			->exec_path_override.memory, MAX_PATH);
#endif
		/* fit it on real size. */
		buffer_resize(((int32_t)strlen(buffer)));
		while (
			buffer[buffer_size - 1] &&
			buffer[buffer_size - 1] != '/' &&
			buffer[buffer_size - 1] != '\\') buffer[--buffer_size] = 0;

		/* remove trailing seperator. */
		xclie_rtrim_self(&(g_xclie_running_args
			->exec_path_override), "/\\");
	}

#undef buffer
#undef buffer_size
#undef buffer_resize
}

void xclie_bootstrap() {
	if (!xclie_test_status(XCSTAT_BOOTSTRAP)) {
#ifdef ZTS
		php_tsrm_startup();
#	ifdef PHP_WIN32
		ZEND_TSRMLS_CACHE_UPDATE();
#	endif
#endif
		zend_signal_startup();
		sapi_startup(g_xclie_module_data);
		g_xclie_status_code |= XCSTAT_BOOTSTRAP;
	}
}

void xclie_translate_arguments() {
	int32_t i = 0;

	if (g_xclie_exec_args->arguments.size) {
		g_global_status->argv_translated = (char**)xclie_gc_alloc(
			sizeof(char*) * g_xclie_exec_args->arguments.size);

		g_global_status->argc_translated = 0;

		for (; i < g_xclie_exec_args->arguments.size; i++) {
			if (!g_xclie_exec_args->arguments.strings[i].size) {
				xclie_string_list_add(&g_xclie_running_args->arguments,
					g_xclie_exec_args->arguments.strings[i].memory,
					g_xclie_exec_args->arguments.strings[i].size);
			}
		}

		for (i = 0; i < g_xclie_running_args->arguments.size; i++) {
			g_global_status->argv_translated[i]
				= g_xclie_running_args->arguments.strings[i].memory;

			++g_global_status->argc_translated;
		}

		/* set executable location. */
		g_xclie_module_data->executable_location
			= g_global_status->argv_translated[0];
	}
}

void xclie_make_path_to_exec_based(
	xclie_string* in_path, xclie_string* out_path)
{
	/* if it isn't set as absolute path, enforce it based on exec-path. */
	if (in_path->memory[0] != '/' &&
		in_path->memory[1] != ':')
	{
		xclie_strcat(out_path,
			g_xclie_running_args->exec_path_override.memory,
			g_xclie_running_args->exec_path_override.size);

		xclie_strcat(out_path, "/", 1);
		xclie_strcat(out_path, in_path->memory, in_path->size);
	}
	else {
		xclie_strcat(out_path, in_path->memory, in_path->size);
	}
}

void xclie_load_ini_settings() {
	xclie_gc_root temp_gc_root;
	xclie_string string_buffer;
	int32_t i = 0, j = 0;
	char* k = 0;

	xclie_gc_root_init(&temp_gc_root);
	xclie_string_init_gc(&temp_gc_root, &string_buffer);

	g_xclie_module_data->php_ini_ignore = 1;
	g_xclie_module_data->php_ini_ignore_cwd = 1;
	//g_xclie_module_data->php_ini_path_override

	if (g_xclie_exec_args->ini_file_override.size) {
		/* if it isn't set as absolute path, enforce it based on exec-path. */
		xclie_make_path_to_exec_based(
			&g_xclie_exec_args->ini_file_override,
			&g_xclie_running_args->ini_file_override);

		/* if file existed, set. */
		if (xclie_test_file(g_xclie_running_args->ini_file_override.memory)) {
			g_xclie_module_data->php_ini_path_override =
				xclie_c_str(&g_xclie_running_args->ini_file_override);
		}
	}

	/* appends default entries. */
	if ((sizeof(XCLIE_DEFAULT_INIT_ENTRIES) - 1) != 0) {
		xclie_strcat(&string_buffer, XCLIE_DEFAULT_INIT_ENTRIES,
			sizeof(XCLIE_DEFAULT_INIT_ENTRIES) - 1);

		xclie_strcat(&string_buffer, PHP_EOL, sizeof(PHP_EOL) - 1);
	}

	if (g_xclie_exec_args->ini_options.size) {
		// g_xclie_exec_args->ini_options.size
		for (i = 0; i < g_xclie_exec_args->ini_options.size; i++) {
			for (j = 0, k = 0; g_xclie_ini_disallowed[j]; j++) {
				if ((k = strstr(g_xclie_exec_args->ini_options.strings[j].memory,
					g_xclie_ini_disallowed[j])) != nullptr)
				{
					if (k != g_xclie_exec_args->ini_options.strings[j].memory) {
						k = nullptr;
					}
				}

				if (k) { break; }
			}

			if (k) { continue; }
			if (g_xclie_exec_args->ini_options.strings[i].size) {
				xclie_strcat(&string_buffer,
					g_xclie_exec_args->ini_options.strings[i].memory,
					g_xclie_exec_args->ini_options.strings[i].size);

				xclie_strcat(&string_buffer, PHP_EOL, sizeof(PHP_EOL) - 1);
			}
		}
	}

	if (g_xclie_exec_args->append_script.size) {
		/* if it isn't set as absolute path, enforce it based on exec-path. */
		xclie_make_path_to_exec_based(
			&g_xclie_exec_args->append_script,
			&g_xclie_running_args->append_script);

		/* if file existed, set. */
		if (xclie_test_file(g_xclie_running_args->append_script.memory)) {
			xclie_strcat(&string_buffer, "auto_append_file=", -1);
			xclie_strcat(&string_buffer,
				g_xclie_running_args->append_script.memory,
				g_xclie_running_args->append_script.size);
		}
	}

	if (g_xclie_exec_args->prepend_script.size) {
		/* if it isn't set as absolute path, enforce it based on exec-path. */
		xclie_make_path_to_exec_based(
			&g_xclie_exec_args->prepend_script,
			&g_xclie_running_args->prepend_script);

		/* if file existed, set. */
		if (xclie_test_file(g_xclie_running_args->prepend_script.memory)) {
			xclie_strcat(&string_buffer, "auto_prepend_file=", -1);
			xclie_strcat(&string_buffer,
				g_xclie_running_args->prepend_script.memory,
				g_xclie_running_args->prepend_script.size);
		}
	}

	if (string_buffer.size) {
		g_xclie_module_data->ini_entries = xclie_c_str(
			xclie_strcat(xclie_string_init(nullptr),
			string_buffer.memory, string_buffer.size));

		xclie_string_deinit(&string_buffer, 0);
		xclie_string_init_gc(&temp_gc_root, &string_buffer);
	}

	xclie_string_deinit(&string_buffer, 0);
	xclie_gc_root_deinit(&temp_gc_root);
}

void xclie_debootstrap() {
	if (xclie_test_status(XCSTAT_BOOTSTRAP)) {
		g_xclie_status_code &= ~XCSTAT_BOOTSTRAP;

		sapi_shutdown();
#ifdef ZTS
		tsrm_shutdown();
#endif
	}
}

void xclie_set_constant(const char* name, const char* value) {
	zend_constant new_constant;

	new_constant.name = zend_string_init(name, strlen(name), 1);
	ZVAL_STRING(&new_constant.value, value);

	zend_register_constant(&new_constant);
}

void xclie_register_std_handles(int32_t no_close) {
	php_stream *st_in, *st_out, *st_err;
	php_stream_context *sc_in = NULL, *sc_out = NULL, *sc_err = NULL;
	zend_constant ct_in, ct_out, ct_err;

	st_in = php_stream_open_wrapper_ex("php://stdin", "rb", 0, NULL, sc_in);
	st_out = php_stream_open_wrapper_ex("php://stdout", "wb", 0, NULL, sc_out);
	st_err = php_stream_open_wrapper_ex("php://stderr", "wb", 0, NULL, sc_err);

	if (no_close) {
		/* do not close stdout and stderr */
		st_out->flags |= PHP_STREAM_FLAG_NO_CLOSE;
		st_err->flags |= PHP_STREAM_FLAG_NO_CLOSE;
	}

	php_stream_to_zval(st_in, &ct_in.value);
	php_stream_to_zval(st_out, &ct_out.value);
	php_stream_to_zval(st_err, &ct_err.value);

	ZEND_CONSTANT_SET_FLAGS(&ct_in, CONST_CS, 0);
	ct_in.name = zend_string_init_interned("STDIN", sizeof("STDIN") - 1, 0);
	zend_register_constant(&ct_in);

	ZEND_CONSTANT_SET_FLAGS(&ct_out, CONST_CS, 0);
	ct_out.name = zend_string_init_interned("STDOUT", sizeof("STDOUT") - 1, 0);
	zend_register_constant(&ct_out);

	ZEND_CONSTANT_SET_FLAGS(&ct_err, CONST_CS, 0);
	ct_err.name = zend_string_init_interned("STDERR", sizeof("STDERR") - 1, 0);
	zend_register_constant(&ct_err);
}

#ifdef PHP_WIN32
DWORD WINAPI xclie_threaded_main(LPVOID p)
#else
void* xclie_threaded_main(void* p)
#endif
{
	/*
		Lock CS/Mutex once,
		And yield thread once.
	*/
#ifdef PHP_WIN32
	HANDLE hEvent;

	EnterCriticalSection(&g_global_status->cs);
	hEvent = g_global_status->event_handle;;
#else
	pthread_mutex_lock(&g_global_status->cs);
#endif

#ifdef PHP_WIN32
	Sleep(0);
	LeaveCriticalSection(&g_global_status->cs);
#else
	pthread_yield();
	pthread_mutex_unlock(&g_global_status->cs);
#endif
	{

		php_execute_script(g_global_status->main_program);

	}
#ifdef PHP_WIN32
	SetEvent(hEvent);
#endif
	return 0;
}

// ------------------------------------------------------------------------------

int xclie_php_startup(sapi_module_struct *sapi_module) {
	if (php_module_startup(sapi_module, NULL, 0) == FAILURE) {
		return FAILURE;
	}

	return SUCCESS;
}

int xclie_php_deactivate(void) {
	xclie_php_flush((void*)0);
	return SUCCESS;
}

size_t xclie_php_ub_write(const char *str, size_t str_length) {
	const char *ptr = str;
	size_t remaining = str_length;
	int ret;

	while (remaining > 0) {
#ifdef PHP_WRITE_STDOUT
		ret = write(STDOUT_FILENO, ptr, remaining);
#else
		ret = (int)fwrite(ptr, 1, MIN(remaining, 16384), stdout);
#endif
		if (ret <= 0) {
			php_handle_aborted_connection();
		}

		ptr += ret;
		remaining -= ret;
	}

	return str_length;
}

void xclie_php_flush(void* ctx) {
	if (fflush(stdout) == EOF) {
		php_handle_aborted_connection();
	}
}
