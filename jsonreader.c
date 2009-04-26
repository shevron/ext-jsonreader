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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_jsonreader.h"

#include "libvktor/vktor.h"

ZEND_DECLARE_MODULE_GLOBALS(jsonreader)


/* {{{ jsonreader_functions[]
 *
 */
/*
const zend_function_entry jsonreader_functions[] = {
	{NULL, NULL, NULL}
};
*/
/* }}} */

/* {{{ jsonreader_module_entry
 */
zend_module_entry jsonreader_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"JSONReader",
	NULL, //jsonreader_functions,
	PHP_MINIT(jsonreader),
	PHP_MSHUTDOWN(jsonreader),
	NULL,
	NULL,
	PHP_MINFO(jsonreader),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1", 
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_JSONREADER
ZEND_GET_MODULE(jsonreader)
#endif

/* {{{ PHP_INI
 */
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("jsonreader.max_nesting_level", "64", PHP_INI_ALL, OnUpdateLong, max_nesting_level, zend_jsonreader_globals, jsonreader_globals)
PHP_INI_END()
/* }}} */

/* {{{ php_jsonreader_init_globals
 */
static void php_jsonreader_init_globals(zend_jsonreader_globals *jsonreader_globals)
{
	jsonreader_globals->max_nesting_level = 64;
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(jsonreader)
{
	REGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(jsonreader)
{
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(jsonreader)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "jsonreader support", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
