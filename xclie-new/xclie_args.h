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

#ifndef __XCLIE_ARGS_H__
#define __XCLIE_ARGS_H__

#ifndef __XCLIE_H__
#include "xclie.h"
#endif

#include "xclie_string.h"

XCLIE_STRUCT(xclie_args, struct _xclie_args {
	/* for keeping argument memory before xclie_args_deinit() called.*/
	xclie_gc_root			gc_root;

	/*
		to enable `xclie_event_issue() and xclie_event_wait()` mechanism.
		in this case, `xclie_cleanup()` should be called.
	*/
	int8_t					as_backend;

	/* register file handles or not.*/
	int8_t					no_std_handles;
	int8_t					no_close_std_handles;
	int8_t					no_block_sigpipe;

	/* execution configurations. */
	xclie_string_list		ini_options;
	xclie_string			ini_file_override;
	xclie_string			exec_path_override;

	/* entry points. */
	xclie_string			prepend_script;
	xclie_string			entry_script;
	xclie_string			append_script;

	/* include pathes. DEPRECATED. UNUSED. */
	xclie_string_list		include_paths;

	xclie_string_list		arguments;

	/* user defined datas on here. */
	union {
		void*				ptr;
		uint32_t			u32;
		uint64_t			u64;
	} data;

	/* private state of xclie. */
	void*					state;

	struct {
		void(*				preinit)(xclie_status*, const char* exec_path);
		void(*				postinit)(xclie_status*);
		void(*				startup)(xclie_status*);
		void(*				shutdown)(xclie_status*);
		void(*				cleanup)(xclie_status*);
	} callbacks;
});

X_INLINE static int32_t xclie_args_init(xclie_args* where, int32_t argc, char** argv) {
	int32_t i = 0;

	if (where) {
		memset(where, 0, sizeof(xclie_args));
		xclie_gc_root_init(&where->gc_root);

		xclie_string_list_init_gc(&where->gc_root, &where->ini_options);
		xclie_string_list_init_gc(&where->gc_root, &where->include_paths);
		xclie_string_list_init_gc(&where->gc_root, &where->arguments);

		xclie_string_init_gc(&where->gc_root, &where->ini_file_override);
		xclie_string_init_gc(&where->gc_root, &where->exec_path_override);
		xclie_string_init_gc(&where->gc_root, &where->prepend_script);
		xclie_string_init_gc(&where->gc_root, &where->entry_script);
		xclie_string_init_gc(&where->gc_root, &where->append_script);

		if (argc && argv) {
			for (i = 0; i < argc; i++) {
				xclie_string_list_add(&where->arguments, argv[i], -1);
			}
		}

		return 1;
	}

	return 0;
}

X_INLINE static int32_t xclie_args_deinit(xclie_args* where) {
	if (where) {
		xclie_string_deinit(&where->ini_file_override, 0);
		xclie_string_deinit(&where->exec_path_override, 0);
		xclie_string_deinit(&where->prepend_script, 0);
		xclie_string_deinit(&where->entry_script, 0);
		xclie_string_deinit(&where->append_script, 0);

		xclie_string_list_deinit(&where->ini_options, 0);
		xclie_string_list_deinit(&where->include_paths, 0);
		xclie_string_list_deinit(&where->arguments, 0);

		xclie_gc_root_deinit(&where->gc_root);
		return 1;
	}

	return 0;
}

#ifdef __cplusplus
namespace xclie {
	class args {
	public:
		args()	{ xclie_args_init(&real_args); }
		~args()	{ xclie_args_deinit(&real_args); }

	private:
		xclie_args real_args;

	public:
		X_INLINE operator xclie_args*() const {
			return &real_args;
		}

		X_INLINE const xclie_args* operator ->() const {
			return &real_args;
		}
	};
}
#endif
#endif
