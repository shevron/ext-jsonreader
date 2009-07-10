dnl $Id$
dnl config.m4 for extension jsonreader

PHP_ARG_ENABLE(jsonreader, whether to enable jsonreader support,
 [  --enable-jsonreader           Enable jsonreader support])

  
if test "$PHP_JSONREADER" != "no"; then
  if test "$ZEND_DEBUG" = "yes"; then
    AC_DEFINE(ENABLE_DEBUG, 1, [Enable debugging information])
  fi

  PHP_NEW_EXTENSION(jsonreader, jsonreader.c libvktor/vktor_unicode.c libvktor/vktor.c, $ext_shared)
fi
