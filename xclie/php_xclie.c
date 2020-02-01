/*
   +----------------------------------------------------------------------+
   | XCLIE Module for Php7                                                |
   +----------------------------------------------------------------------+
   | License: GPLv3                                                       |
   | This file is not a part of official distribution of PHP.			  |
   +----------------------------------------------------------------------+
   | Author: Jaehoon Joe <jay94ks@gmail.com>                              |
   | https://github.com/jay94ks/php-xclie                                 |
   +----------------------------------------------------------------------+
*/

#define __PHP_XCLIE__

#include <main/php.h>
#include <main/SAPI.h>
#include <main/php_main.h>
#include <main/php_variables.h>
#include <main/php_ini.h>
#include <zend_ini.h>

#include "ext/standard/php_standard.h"

#ifdef PHP_WIN32
#include <io.h>
#include <fcntl.h>
#endif

#include "php_xclie.h"
#include <direct.h>

#define XCLIE_CONSTANT_EXEC_DIR			"__EXEC_DIR__"
#define XCLIE_CONSTANT_EXEC_CWD			"__EXEC_CWD__"

#if defined(PHP_WIN32) && defined(ZTS)
ZEND_TSRMLS_CACHE_DEFINE()
#endif

static struct xclie_exec* g_exec_args = 0;


typedef struct { char* head; int size; } xclie_string;

inline static char* xclie_string_tail(xclie_string* string) {
	return string->head ? string->head + string->size : 0;
}

inline static char* xclie_c_str(xclie_string* string) {
	return (string && string->size) ? string->head : 0;
}

inline static int xclie_file_exists(const char* file) {
	struct stat _buf;

	if (!stat(file, &_buf) &&
		(_buf.st_mode & S_IFMT) == S_IFREG)
	{
		return 1;
	}

	return 0;
}

inline static void xclie_string_init(xclie_string* string, const char* in_string) {
	int init_size = in_string ? (int) strlen(in_string) : 0;
	string->head = 0; string->size = 0;

	if (in_string && init_size > 0) {
		string->head = (char*)malloc(init_size + 1);

		memset(string->head, 0, init_size + 1);
		strcpy(string->head, in_string);

		string->size = init_size;
	}
}

inline static void xclie_string_deinit(xclie_string* string) {
	if (string->head) { free(string->head); }
	string->head = 0; string->size = 0;
}

inline static void xclie_string_append(xclie_string* string, const char* in_string) {
	int in_size = in_string ? (int) strlen(in_string) : 0;
	char* old_head = string->head;

	if (in_size > 0) {
		string->head = (char*)malloc(string->size + in_size + 1);
		memset(string->head + string->size, 0, in_size + 1);

		if (old_head) {
			memcpy(string->head, old_head, string->size);
			free(old_head);
		}

		strcpy(string->head + string->size, in_string);
		string->size += in_size;
	}
}

inline static int xclie_get_exec_path(char* buf, int no_filename)
{
	char _buf[MAX_PATH + 1];
	int i = 0;

	if (buf) {
		memset(_buf, 0, sizeof(_buf));

#ifdef PHP_WIN32
		GetModuleFileNameA(NULL, _buf, sizeof(_buf) - 1);
#else
		readlink("/proc/self/exe", _buf, sizeof(_buf) - 1);
#endif

		strcpy(buf, _buf);
		*(buf + strlen(_buf)) = 0;

		if (no_filename) {
			memset(_buf, 0, sizeof(_buf));
			strcpy(_buf, buf);

			i = (int)(strlen(_buf)) - 1;

			while ( _buf[i] && _buf[i] != '/' && _buf[i] != '\\') { _buf[i--] = 0; }
			if (_buf[i] == '/' || _buf[i] == '\\') { _buf[i] = 0; }

			strcpy(buf, _buf);
			*(buf + strlen(_buf)) = 0;
		}
	}

	return buf && buf[0] ? 0 : -1;
}


static char* php_xclie_read_cookies(void) { return NULL; }
static int php_xclie_header_handler(sapi_header_struct *h, sapi_header_op_enum op, sapi_headers_struct *s) { return 0; }
static void php_xclie_send_header(sapi_header_struct *sapi_header, void *server_context) { }
static int php_xclie_send_headers(sapi_headers_struct *sapi_headers) { return SAPI_HEADER_SENT_SUCCESSFULLY; }

static void php_xclie_flush(void *server_context)
{
	if (g_exec_args && g_exec_args->on_unbuffered_out) {
		if (g_exec_args->on_unbuffered_flush(g_exec_args) < 0) {
			php_handle_aborted_connection();
		}
	}

	else if (fflush(stdout) == EOF) {
		php_handle_aborted_connection();
	}
}

static int php_xclie_deactivate(void) {
	php_xclie_flush((void*) 0);
	//fflush(stdout);
	return SUCCESS;
}

static void php_xclie_register_variables(zval *track_vars_array) {
	php_import_environment_variables(track_vars_array);
}

static int php_xclie_startup(sapi_module_struct *sapi_module) {
	if (php_module_startup(sapi_module, NULL, 0)==FAILURE) {
		return FAILURE;
	}

	return SUCCESS;
}

#define XCLIE_INI_HTML_ERRORS			"html_errors"
#define XCLIE_INI_REGISTER_ARGC_ARGV	"register_argc_argv"
#define XCLIE_INI_IMPLICIT_FLUSH		"implicit_flush"
#define XCLIE_INI_OUTPUT_BUFFERING		"output_buffering"
#define XCLIE_INI_MAX_EXECUTION_TIME	"max_execution_time"
#define XCLIE_INI_MAX_INPUT_TIME		"max_input_time"

const char* XCLIE_DEFAULT_INI_VALUES =
	XCLIE_INI_HTML_ERRORS "=0\n"
	XCLIE_INI_REGISTER_ARGC_ARGV "=1\n"
	XCLIE_INI_IMPLICIT_FLUSH "=1\n"
	XCLIE_INI_OUTPUT_BUFFERING "=0\n"
	XCLIE_INI_MAX_EXECUTION_TIME "=0\n"
	XCLIE_INI_MAX_INPUT_TIME "=-1\n";

const char* XCLIE_DENIED_ENTRIES[] = {
	XCLIE_INI_HTML_ERRORS,
	XCLIE_INI_REGISTER_ARGC_ARGV,
	XCLIE_INI_IMPLICIT_FLUSH,
	XCLIE_INI_OUTPUT_BUFFERING,
	XCLIE_INI_MAX_EXECUTION_TIME,
	XCLIE_INI_MAX_INPUT_TIME,

	NULL
};

static void php_xclie_ini_defaults(HashTable* configs) {
	zval tmp;

	ZVAL_NEW_STR(&tmp, zend_string_init("0", 1, 1));
	zend_hash_str_update(configs, XCLIE_INI_HTML_ERRORS, sizeof(XCLIE_INI_HTML_ERRORS) - 1, &tmp);

	ZVAL_NEW_STR(&tmp, zend_string_init("1", 1, 1));
	zend_hash_str_update(configs, XCLIE_INI_REGISTER_ARGC_ARGV, sizeof(XCLIE_INI_REGISTER_ARGC_ARGV) - 1, &tmp);

	ZVAL_NEW_STR(&tmp, zend_string_init("0", 1, 1));
	zend_hash_str_update(configs, XCLIE_INI_OUTPUT_BUFFERING, sizeof(XCLIE_INI_OUTPUT_BUFFERING) - 1, &tmp);

	ZVAL_NEW_STR(&tmp, zend_string_init("0", 1, 1));
	zend_hash_str_update(configs, XCLIE_INI_MAX_EXECUTION_TIME, sizeof(XCLIE_INI_MAX_EXECUTION_TIME) - 1, &tmp);

	ZVAL_NEW_STR(&tmp, zend_string_init("-1", 2, 1));
	zend_hash_str_update(configs, XCLIE_INI_MAX_INPUT_TIME, sizeof(XCLIE_INI_MAX_INPUT_TIME) - 1, &tmp);

	ZVAL_NEW_STR(&tmp, zend_string_init("1", 1, 1));
	zend_hash_str_update(configs, XCLIE_INI_IMPLICIT_FLUSH, sizeof(XCLIE_INI_IMPLICIT_FLUSH) - 1, &tmp);
}

void php_xclie_log_message(char *message, int syslog_type_int);
size_t php_xclie_ub_write(const char *str, size_t str_length);

XCLIE_API extern sapi_module_struct php_xclie_module = {
	"xclie",                       /* name */
	"PHP eXtendable CLI Embeded",  /* pretty name */

	php_xclie_startup,              /* startup */
	php_module_shutdown_wrapper,   /* shutdown */

	NULL,                          /* activate */
	php_xclie_deactivate,           /* deactivate */

	php_xclie_ub_write,             /* unbuffered write */
	php_xclie_flush,                /* flush */
	NULL,                          /* get uid */
	NULL,                          /* getenv */

	php_error,                     /* error handler */

	php_xclie_header_handler,      /* header handler */
	php_xclie_send_headers,        /* send headers handler */
	php_xclie_send_header,         /* send header handler */

	NULL,                          /* read POST data */
	php_xclie_read_cookies,         /* read Cookies */

	php_xclie_register_variables,   /* register server variables */
	php_xclie_log_message,          /* Log message */
	NULL,							/* Get request time */
	NULL,							/* Child terminate */

	STANDARD_SAPI_MODULE_PROPERTIES
};
/* }}} */

/* {{{ arginfo ext/standard/dl.c */
ZEND_BEGIN_ARG_INFO(arginfo_dl, 0)
	ZEND_ARG_INFO(0, extension_filename)
ZEND_END_ARG_INFO()
/* }}} */

static const zend_function_entry additional_functions[] = {
	ZEND_FE(dl, arginfo_dl)
	{NULL, NULL, NULL}
};

void xclie_handle_ini_settings(sapi_module_struct* self,
	const char* exec_path, xclie_string* ini_buffer, xclie_string* dyn_name_buffer);

void xclie_register_exec_dir_constant(const char* exec_path);
void xclie_register_std_handles(int no_close);

int xclie_load_entry_file(const char* exec_path, zend_file_handle* out_handle, xclie_string* buffer);

XCLIE_API extern int xclie_run(struct xclie_exec* args) {
	sapi_module_struct* self = &php_xclie_module;
	zend_file_handle file_handle;
	zend_llist global_vars;

	int exitCode = XCLIX_SUCCESS;
	int stepCode = 0; /* 0: sapi, 1: module, 2: request, 3: scripting. */

	xclie_string dyn_ini_options;
	xclie_string dyn_ini_file_overrides;
	xclie_string dyn_entry_script;

	char exec_path[MAX_PATH + 1];

	if (!args) { return XCLIX_NULL_ARGS; }
	else if (xclie_get_exec_path(exec_path, 1)) {
		return XCLIX_NO_EXEC_DIR;
	}

	g_exec_args = args;

#if defined(SIGPIPE) && defined(SIG_IGN)
	signal(SIGPIPE, SIG_IGN); 
	/* ignore SIGPIPE in standalone mode so that sockets created via fsockopen()
	 don't kill PHP if the remote site closes it.  in apache|apxs mode apache
	 does that for us!  thies@thieso.net 20000419 */
#endif

#ifdef ZTS
	php_tsrm_startup();
# ifdef PHP_WIN32
	ZEND_TSRMLS_CACHE_UPDATE();
# endif
#endif

	zend_signal_startup();
	sapi_startup(&php_xclie_module);
	stepCode = 0; /* SAPI */

#ifdef PHP_WIN32
	_fmode = _O_BINARY;						/*sets default for file streams to binary */
	setmode(_fileno(stdin), O_BINARY);		/* make the stdio mode be binary */
	setmode(_fileno(stdout), O_BINARY);		/* make the stdio mode be binary */
	setmode(_fileno(stderr), O_BINARY);		/* make the stdio mode be binary */
#endif

	while (1) {
		if (args->preinit) {
			args->preinit(args, exec_path);
		}

		xclie_string_init(&dyn_ini_options, 0);
		xclie_string_init(&dyn_ini_file_overrides, 0);
		xclie_string_init(&dyn_entry_script, 0);

		/* handle all ini related settings. */
		xclie_handle_ini_settings(self, exec_path,
			&dyn_ini_options, &dyn_ini_file_overrides);

		if (args->argv && args->argv[0]) {
			self->executable_location = args->argv[0];
		}

		self->additional_functions = additional_functions;
		if (self->startup(self) == FAILURE) {
			exitCode = XCLIX_CANT_STARTUP;
			break;
		}

		stepCode = 1;  /* MODULE */
		zend_llist_init(&global_vars, sizeof(char *), NULL, 0);

		/* keep working directory to exec location. */
		SG(options) |= SAPI_OPTION_NO_CHDIR;
		SG(request_info).argc = args->argc;
		SG(request_info).argv = args->argv;

		if (args->postinit) {
			args->postinit(args);
		}

		/* load file if possible. */
		if (xclie_load_entry_file(exec_path,
			&file_handle, &dyn_entry_script))
		{
			exitCode = XCLIX_NO_SUCH_FILE;
			break;
		}

		//-----

		if (php_request_startup() == FAILURE) {
			exitCode = XCLIX_CANT_STARTUP;
			break;
		}

		stepCode = 2;  /* REQUEST */

		SG(headers_sent) = 1;
		SG(request_info).no_headers = 1;

		/* expose exec_path to constant, __EXEC_DIR__. */
		xclie_register_exec_dir_constant(exec_path);
		if (!args->no_register_std_handles) {
			/* register stdin, stdout, stderr. */
			xclie_register_std_handles(args->no_close_std_handles);
		}

		if (args->startup) {
			args->startup(args);
		}

		stepCode = 3;  /* SCRIPT */

		if (SG(request_info).path_translated) {
			if (php_fopen_primary_script(&file_handle) == FAILURE) {
				if (errno == EACCES) {
					exitCode = XCLIX_NO_PERMISSION;
				}
				else {
					exitCode = XCLIX_NO_SUCH_FILE;
				}

				if (SG(request_info).path_translated) {
					SG(request_info).path_translated = NULL;
				}
			}
			else {
				php_execute_script(&file_handle);
			}
		}
		else {
			exitCode = XCLIX_NO_FILE_SPEC;
		}

		break;
	}

	switch (stepCode) {
	case 3: /* SCRIPTED, */
		if (args->shutdown) {
			args->shutdown(args);
		}

	case 2: /* REQUESTED, */
		php_request_shutdown((void *)0);

	case 1: /* MODULED*/
		php_module_shutdown();

	case 0: /* SAPI*/
	default:
		break;
	}

	if (args->cleanup) {
		args->cleanup(args);
	}
	
	sapi_shutdown();
#ifdef ZTS
    tsrm_shutdown();
#endif

	if (g_exec_args) {
		g_exec_args = 0;
	}

	xclie_string_deinit(&dyn_ini_options);
	xclie_string_deinit(&dyn_ini_file_overrides);
	xclie_string_deinit(&dyn_entry_script);

	self->ini_entries = NULL;

	return exitCode;
}

void xclie_ini_append_lines(xclie_string* buffer);
void xclie_ini_append_includes(const char* exec_path, xclie_string* buffer);
void xclie_ini_append_appendix_script(const char* script,
	const char* type, const char* exec_path, xclie_string* buffer);

void xclie_handle_ini_settings(sapi_module_struct* self, const char* exec_path,
	xclie_string* ini_buffer, xclie_string* dyn_name_buffer)
{
	/* set default ini entries. */
	xclie_string_append(ini_buffer, XCLIE_DEFAULT_INI_VALUES);
	xclie_string_append(ini_buffer, PHP_EOL);

	/* include pathes. */
	if (g_exec_args->include_paths) {
		xclie_ini_append_includes(exec_path, ini_buffer);
	}

	/* prepend and append files. */
	xclie_ini_append_appendix_script(g_exec_args->prepend_script,
		"auto_prepend_file", exec_path, ini_buffer);

	xclie_ini_append_appendix_script(g_exec_args->append_script,
		"auto_append_file", exec_path, ini_buffer);

	/* append all option lines. */
	xclie_ini_append_lines(ini_buffer);

	self->phpinfo_as_text = 1;
	self->php_ini_ignore_cwd = 1;
	self->php_ini_ignore = 1;
	self->ini_entries = xclie_c_str(ini_buffer);

	if (g_exec_args->ini_file_overrides) {
		if (xclie_file_exists(g_exec_args->ini_file_overrides)) {
			self->php_ini_path_override = (char*)g_exec_args->ini_file_overrides;
			self->php_ini_ignore = 0;
		}

		else if (g_exec_args->ini_file_overrides[0] != '/' &&
			g_exec_args->ini_file_overrides[1] != ':')
		{
			xclie_string_append(dyn_name_buffer, exec_path);
			xclie_string_append(dyn_name_buffer, "/");
			xclie_string_append(dyn_name_buffer, g_exec_args->ini_file_overrides);

			if (xclie_file_exists(xclie_c_str(dyn_name_buffer))) {
				self->php_ini_path_override = xclie_c_str(dyn_name_buffer);
				self->php_ini_ignore = 0;
			}
		}
	}

	self->ini_defaults = php_xclie_ini_defaults;
}

void xclie_ini_append_lines(xclie_string* buffer) {
	int i = 0, j = 0, is_legal = 0;

	if (g_exec_args->ini_option_lines) {
		while (g_exec_args->ini_option_lines[i]) {
			j = 0; is_legal = 1;

			while (XCLIE_DENIED_ENTRIES[j]) {
				if (strstr(g_exec_args->ini_option_lines[i++],
					XCLIE_DENIED_ENTRIES[j]))
				{
					// illegal entity.
					is_legal = 0;
					break;
				}

				++j;
			}

			if (is_legal) {
				xclie_string_append(buffer, g_exec_args->ini_option_lines[i++]);
				xclie_string_append(buffer, PHP_EOL);
			}
		}
	}
}

void xclie_ini_append_includes(const char* exec_path, xclie_string* buffer) {
	int i = 0;

	xclie_string_append(buffer, "include_path=\".");
	while (g_exec_args->include_paths[i]) {
		xclie_string_append(buffer, ";");
		xclie_string_append(buffer, g_exec_args->include_paths[i++]);
	}

	xclie_string_append(buffer, ";");
	xclie_string_append(buffer, exec_path);
	xclie_string_append(buffer, "\"" PHP_EOL);
}

void xclie_ini_append_appendix_script(const char* script,
	const char* type, const char* exec_path, xclie_string* buffer)
{
	int pends_flag = 0;

	while (script) {
		if (!xclie_file_exists(script)) {
			if (script[0] != '/' &&	// root '/'
				script[1] != ':')		// C:\, D:\...
			{
				xclie_string_append(buffer, type);
				xclie_string_append(buffer, "=");
				pends_flag = 1;

				xclie_string_append(buffer, exec_path);
				xclie_string_append(buffer, "/");
			}
			else {
				break;
			}
		}

		if (!pends_flag) {
			xclie_string_append(buffer, type);
			xclie_string_append(buffer, "=");
		}

		xclie_string_append(buffer, script);
		xclie_string_append(buffer, PHP_EOL);
		break;
	}
}
/*

#define XCLIE_CONSTANT_EXEC_DIR			"__EXEC_DIR__"
#define XCLIE_CONSTANT_EXEC_CWD			"__EXEC_CWD__"
*/
void xclie_register_exec_dir_constant(const char* exec_path) {
	char cwd_buf[MAX_PATH + 1];
	zend_constant exec_dir, exec_cwd;

	memset(cwd_buf, 0, sizeof(cwd_buf));
	getcwd(cwd_buf, sizeof(cwd_buf) - 1);

	exec_dir.name = zend_string_init(XCLIE_CONSTANT_EXEC_DIR,
		sizeof(XCLIE_CONSTANT_EXEC_DIR) - 1, 1);

	exec_cwd.name = zend_string_init(XCLIE_CONSTANT_EXEC_CWD,
		sizeof(XCLIE_CONSTANT_EXEC_CWD) - 1, 1);

	ZVAL_STRING(&exec_dir.value, exec_path);
	ZVAL_STRING(&exec_cwd.value, cwd_buf);

	zend_register_constant(&exec_dir);
	zend_register_constant(&exec_cwd);
}

void xclie_register_std_handles(int no_close) {
	php_stream *st_in, *st_out, *st_err;
	php_stream_context *sc_in = NULL, *sc_out = NULL, *sc_err = NULL;
	zend_constant ct_in, ct_out, ct_err;

	st_in = php_stream_open_wrapper_ex("php://stdin", "rb", 0, NULL, sc_in);
	st_out = php_stream_open_wrapper_ex("php://stdout", "wb", 0, NULL, sc_out);
	st_err = php_stream_open_wrapper_ex("php://stderr", "wb", 0, NULL, sc_err);

	if (st_in == NULL || st_out == NULL || st_err == NULL) {
		if (st_in) php_stream_close(st_in);
		if (st_out) php_stream_close(st_out);
		if (st_err) php_stream_close(st_err);
		return;
	}

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

int xclie_load_entry_file(
	const char* exec_path, zend_file_handle* out_handle, xclie_string* buffer)
{
	if (g_exec_args->entry_script) {
		if (!xclie_file_exists(g_exec_args->entry_script)) {
			if (g_exec_args->entry_script[0] != '/' &&
				g_exec_args->entry_script[1] != ':')
			{
				xclie_string_append(buffer, exec_path);
				xclie_string_append(buffer, "/");
				xclie_string_append(buffer, g_exec_args->entry_script);
			}

			if (!xclie_file_exists(xclie_c_str(buffer))) {
				return -1;
			}
			else {
				SG(request_info).path_translated = xclie_c_str(buffer);
				zend_stream_init_filename(out_handle, xclie_c_str(buffer));
			}
		}

		else {
			SG(request_info).path_translated = (char*)g_exec_args->entry_script;
			zend_stream_init_filename(out_handle, g_exec_args->entry_script);
		}
	}

	return 0;
}

size_t php_xclie_ub_write(const char *str, size_t str_length)
{
	const char *ptr = str;
	size_t remaining = str_length;
	int ret;

	while (remaining > 0) {
		if (g_exec_args && g_exec_args->on_unbuffered_out) {
			ret = g_exec_args->on_unbuffered_out(g_exec_args, ptr, (int)(MIN(remaining, 16384)));
		}

		else {
#ifdef PHP_WRITE_STDOUT
			ret = write(STDOUT_FILENO, ptr, remaining);
#else
			ret = (int) fwrite(ptr, 1, MIN(remaining, 16384), stdout);
#endif
			if (ret <= 0) {
				php_handle_aborted_connection();
			}
		}

		ptr += ret;
		remaining -= ret;
	}

	return str_length;
}

void php_xclie_log_message(char *message, int syslog_type_int)
{
	if (g_exec_args && g_exec_args->on_log_message) {
		g_exec_args->on_log_message(g_exec_args, message);
	}
	else {
		fprintf(stderr, "%s\n", message);
	}
}
