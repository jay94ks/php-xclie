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
#error	"bits/xclie_exit_code.h can not be standalone!"
#else

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

	/* PHP backend running successfully. */
	XCLIX_ON_BACKGROUND,

	/* PHP backend already running. */
	XCLIX_ALREADY

};

#endif
