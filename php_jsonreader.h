/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2008 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id: header,v 1.16.2.1.2.1.2.1 2008/02/07 19:39:50 iliaa Exp $ */

#ifndef PHP_JSONREADER_H
#define PHP_JSONREADER_H

extern zend_module_entry jsonreader_module_entry;
#define phpext_jsonreader_ptr &jsonreader_module_entry

#ifdef PHP_WIN32
#	define PHP_JSONREADER_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_JSONREADER_API __attribute__ ((visibility("default")))
#else
#	define PHP_JSONREADER_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(jsonreader);
PHP_MSHUTDOWN_FUNCTION(jsonreader);
PHP_MINFO_FUNCTION(jsonreader);

ZEND_BEGIN_MODULE_GLOBALS(jsonreader)
	long  max_nesting_level;
ZEND_END_MODULE_GLOBALS(jsonreader)

/* In every utility function you add that needs to use variables 
   in php_jsonreader_globals, call TSRMLS_FETCH(); after declaring other 
   variables used by that function, or better yet, pass in TSRMLS_CC
   after the last function argument and declare your utility function
   with TSRMLS_DC after the last declared argument.  Always refer to
   the globals in your function as JSONREADER_G(variable).  You are 
   encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/

#ifdef ZTS
#define JSONREADER_G(v) TSRMG(jsonreader_globals_id, zend_jsonreader_globals *, v)
#else
#define JSONREADER_G(v) (jsonreader_globals.v)
#endif

#endif	/* PHP_JSONREADER_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
