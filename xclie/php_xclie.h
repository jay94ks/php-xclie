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

#ifndef _PHP_XCLIE_H_
#define _PHP_XCLIE_H_

#ifdef __PHP_XCLIE__
	#ifndef PHP_WIN32
		#define XCLIE_API SAPI_API
	#else
		#define XCLIE_API
	#endif
	#ifdef ZTS
	ZEND_TSRMLS_CACHE_EXTERN()
	#endif
#else
	#if defined(WIN32) || defined(WIN64) || \
		defined(_WIN32) || defined(_WIN64)
		#define XCLIE_API	__declspec(dllimport)
	#else
		#define XCLIE_API
	#endif
#endif

/* XCLIE execution parameters. */
struct xclie_exec;
struct xclie_exec {
	/* execution configurations. */
	const char**			ini_option_lines;
	const char*				ini_file_overrides;

	/* register file handles or not.*/
	int						no_register_std_handles;
	int						no_close_std_handles;
	
	/* entry points. */
	const char*				prepend_script;
	const char*				entry_script;
	const char*				append_script;

	/* include pathes. */
	const char**			include_paths;
	
	/* execution parameters. */
	int						argc;
	char**					argv;
	
	/* user defined datas on here. */
	union {
		void*				ptr;
		unsigned int		u32;
		unsigned long long	u64;
	} data;
	
	/* private state of xclie. */
	void*					state;
	
	/*
		calling order:
		-. sapi_startup

		1. preinit()

		-. module startup

		2. postinit()

		-. php_request_startup

		3. startup()
		
		-. hardcorded_script
		-. entry_script
		
		4. shutdown()

		-. php_request_shutdown
		-. php_module_shutdown

		5. cleanup()

		-. sapi_shutdown
	*/
	void(*preinit)(struct xclie_exec*, const char* exec_path);
	void(*postinit)(struct xclie_exec*);
	void(*startup)(struct xclie_exec*);
	void(*shutdown)(struct xclie_exec*);
	void(*cleanup)(struct xclie_exec*);

	/*
		to get generated output.
		xclie will call this like `write()` function.

		if this function specified,
		almost output through stdout will be redirected to this function.
	*/
	int (*on_unbuffered_out)(struct xclie_exec*, const char*, unsigned int);
	void(*on_log_message)(struct xclie_exec*, const char*);

	/*
		to flush generated output.
		on_unbuffered_out should be specified. or not, this will not be called.
		(negative return value is `abort`)
	*/
	int(*on_unbuffered_flush)(struct xclie_exec*);
};

/*
	Exit codes of xclie_run.
*/
enum {
	XCLIX_SUCCESS = 0,

	/* In calling, xclie_run(ARGS), the ARGS is null pointer specified. */
	XCLIX_NULL_ARGS,

	/* XCLIE couldn't predict the EXEC root. */
	XCLIX_NO_EXEC_DIR,

	/* Can't startup php native backend. */
	XCLIX_CANT_STARTUP,

	/* No file specified. */
	XCLIX_NO_FILE_SPEC,

	/* No permission to access the specified file. */
	XCLIX_NO_PERMISSION,

	/* No such file existed. */
	XCLIX_NO_SUCH_FILE,

};

#ifndef __PHP_XCLIE__
	#ifdef __cplusplus
	extern "C" {
	#endif
#else
	BEGIN_EXTERN_C()
#endif

extern int xclie_run(struct xclie_exec* args);

#ifdef __PHP_XCLIE__
	END_EXTERN_C()
#else
	#ifdef __cplusplus
	}
	#endif
#endif

#endif /* _PHP_XCLIE_H_ */
