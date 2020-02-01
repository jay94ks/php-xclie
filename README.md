# php-xclie

PHP eXtendable CLI Embeding library.
XCLIE is a library to create custom PHP interpreter executable.

# License

the php-xclie module itself is under the LGPLv2 license. read it first and enjoy xclie.

# Concept

A library for writing your own php interpreter, clearly separated 
(at the source code level, simply, hard-coded) from the php installation 
that works with Apache or nginx.

# Supported Platforms

1. Linux and almost unix systems. (except iOS, android)
2. Windows

# Example
```
int main(int argc, char** argv) {
	struct xclie_exec args = { 0, };
	int retVal = 0;

	args.prepend_script = "autoloader.inc.php";
	args.entry_script = "main.inc.php";

	args.argc = argc;
	args.argv = argv;

	if ((retVal = xclie_run(&args)) != XCLIX_SUCCESS) {
		switch (retVal) {
		case XCLIX_CANT_STARTUP:
			fprintf(stderr, "Unknown error: PHP startup routines failure.\n");
			break;

		case XCLIX_NO_FILE_SPEC:
			fprintf(stderr, "No file specified. are you sure that \"entry_script\" set?\n");
			break;

		case XCLIX_NO_PERMISSION:
			fprintf(stderr, "No permission to read the entry script: %s.\n", args.entry_script);
			break;

		case XCLIX_NO_SUCH_FILE:
			fprintf(stderr, "No such file existed: %s.\n", args.entry_script);
			break;
		}

		return -1;
	}

	return 0;
}```

# How to build

1. Download the official PHP copy on your working directory.

2. Copy `xclie` directory into PHP's `sapi` directory.

3. Configure it with `--enable-xclie=static` switch.

on Windows system, `--enable-xclie=shared` will be automatically changed to `--enable-xclie=static`,
and other systems, both are working well.

4. Make it and install.

and then, `php7xclie.lib` (or `php7xclie.so`) file will be installed at:
```
	`dirname $(which php)`/php7xclie.so
```

5. Write your own codes with `php_xclie.h` header:

No more headers are required. `php_xclie.h` header is enough to do all.
even, just copying it to your own directory also working well.
because it has no direct dependency to PHP internal types as header.
```
#include <php_xclie.h>
int main(int argc, char** argv) {
	.....

	return xclie_run( ... );
}

```

6. Compile like below:
```
# gcc your_php_interpreter.c -L `dirname $(which php)` -lphp7xclie -o your_php_interpreter
```
(C++ also supported)


# APIs?

Currently, there is only one API defined. (and one structure + few callbacks).

```
extern int xclie_run(struct xclie_exec* args);

struct xclie_exec {
	/* execution configurations. */
	const char**			ini_option_lines;		
	// --> NULL terminated array.

	const char*				ini_file_overrides;		
	// --> FILE-path. if this field set NULL, php.ini file will be never loaded.

	/* register file handles or not.*/
	int						no_register_std_handles;
	// --> to remove declarartion of php://stdin, php://stdout, php:://stderr, set 1.

	int						no_close_std_handles;
	// --> to disallow PHP can close `stdin, stdout, stderr` or not. (1 for disallow)
	
	/* entry points. */
	const char*				prepend_script;
	// --> prepend script file name. (this file will be invoked before entry-script)

	const char*				entry_script;
	// --> entry (main) script file name.
	//     all options from command line are not WORKING like `php-cli, php-cgi` something.
	//     there are no CLI feed-able options defined by `XCLIE`.

	const char*				append_script;
	// --> append script file name. (this file will be invoked at end of entry-script)

	/* include pathes. */
	const char**			include_paths;
	// --> NULL terminated array.
	//     e.g. { "/usr/local/my_php_libs", NULL }
	
	/* execution parameters. */
	int						argc;
	char**					argv;
	// --> just passing your argc and argv from main(...)
	//     these arguments will be set to `$argc` and `$argv`. 
	
	/* user defined datas on here. */
	union {
		void*				ptr;
		unsigned int		u32;
		unsigned long long	u64;
	} data;
	// --> set any datas. as you want.
	
	/* private state of xclie. */
	void*					state;
	// --> this field is for C++ wrapper. but, not available yet :)
	
	/* callbacks. */
	void(*preinit)(struct xclie_exec*, const char* exec_path);
	// --> called before `xclie` module loaded.

	void(*postinit)(struct xclie_exec*);
	// --> called after `xclie` module loaded.

	void(*startup)(struct xclie_exec*);
	// --> called before invoking scripts.

	void(*shutdown)(struct xclie_exec*);
	// --> called after invoking scripts.

	void(*cleanup)(struct xclie_exec*);
	// --> called before returning. (from `xclie_run(...)` function)

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
```

