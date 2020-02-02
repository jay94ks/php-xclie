PHP_ARG_ENABLE([xclie],,
  [AS_HELP_STRING([[--enable-xclie[=TYPE]]],
    [EXPERIMENTAL: Enable building of PHP eXtendable CLI Embeded library TYPE is either
    'shared' or 'static'. [TYPE=shared]])],
  [no],
  [no])

AC_MSG_CHECKING([for PHP eXtendable CLI Embeded library support])

if test "$PHP_XCLIE" != "no"; then
  case "$PHP_XCLIE" in
    yes|shared)
      PHP_XCLIE_TYPE=shared
      INSTALL_IT="\$(mkinstalldirs) \$(INSTALL_ROOT)\$(prefix)/lib; \$(INSTALL) -m 0755 $SAPI_SHARED \$(INSTALL_ROOT)\$(prefix)/lib"
      ;;
    static)
      PHP_XCLIE_TYPE=static
      INSTALL_IT="\$(mkinstalldirs) \$(INSTALL_ROOT)\$(prefix)/lib; \$(INSTALL) -m 0644 $SAPI_STATIC \$(INSTALL_ROOT)\$(prefix)/lib"
      ;;
    *)
      PHP_XCLIE_TYPE=no
      ;;
  esac
  if test "$PHP_XCLIE_TYPE" != "no"; then
    PHP_SELECT_SAPI(xclie, $PHP_XCLIE_TYPE,
      xclie.c xclie_args.c xclie_event.c xclie_string.c,
      -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 -D__XCLIE__=1 -lpthread)
    PHP_INSTALL_HEADERS(
      [sapi/xclie/xclie.h],
      [sapi/xclie/xclie_args.h],
      [sapi/xclie/xclie_event.h],
      [sapi/xclie/xclie_string.h],
      [sapi/xclie/bits/xclie_gc.h],
      [sapi/xclie/bits/xclie_status.h],
      [sapi/xclie/bits/xclie_exit_code.h])
  fi
  AC_MSG_RESULT([$PHP_XCLIE_TYPE])
else
  AC_MSG_RESULT(no)
fi
