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

#include "libvktor/vktor.h"

extern zend_module_entry jsonreader_module_entry;
#define phpext_jsonreader_ptr &jsonreader_module_entry

#ifdef PHP_WIN32
#	define PHP_JSONREADER_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_JSONREADER_API __attribute__ ((visibility("default")))
#else
#define PHP_JSONREADER_API
zend_module_entry *get_module(void);
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#define JSONREADER_DEFAULT_MAX_DEPTH       64
#define JSONREADER_DEFAULT_MAX_READ_BUFFER 4096
#define STRINGIFY(x) #x
#define XSTRINGIFY(a) STRINGIFY(a)

ZEND_BEGIN_MODULE_GLOBALS(jsonreader)
	long  max_depth;
	long  read_buffer;
ZEND_END_MODULE_GLOBALS(jsonreader)

PHP_MINIT_FUNCTION(jsonreader);
PHP_GINIT_FUNCTION(jsonreader);
PHP_MSHUTDOWN_FUNCTION(jsonreader);
PHP_MINFO_FUNCTION(jsonreader);

extern zend_class_entry *jsonreader_ce;

PHP_METHOD(jsonreader, read);
PHP_METHOD(jsonreader, close);
PHP_METHOD(jsonreader, open);
PHP_METHOD(jsonreader, __construct);

typedef struct _jsonreader_object {
	php_stream   *stream;
	void         *internal_stream;
	vktor_parser *parser;
	long          max_depth;
	long          read_buffer;
	int           errmode;
	zend_object   std;
} jsonreader_object;

#define FETCH_JSON_OBJECT_FROM_OBJ(obj) jsonreader_object *intern = (jsonreader_object *)((char *)(obj) - XtOffsetOf(struct _jsonreader_object, std));
#define FETCH_JSON_OBJECT_FROM_ZV(obj) FETCH_JSON_OBJECT_FROM_OBJ(Z_OBJ_P(obj))
#define FETCH_JSON_OBJECT FETCH_JSON_OBJECT_FROM_ZV(getThis())

#define JSONREADER_REG_CLASS_CONST_L(name, value) \
	zend_declare_class_constant_long(jsonreader_ce, name, strlen(name), (long) value)

#define JSONREADER_VALUE_TOKEN VKTOR_T_NULL  | \
							   VKTOR_T_TRUE  | \
							   VKTOR_T_FALSE | \
							   VKTOR_T_INT   | \
							   VKTOR_T_FLOAT | \
							   VKTOR_T_STRING

/* {{{ attribute keys and possible values */
enum {
	ATTR_MAX_DEPTH = 1,
	ATTR_READ_BUFF,
	ATTR_ERRMODE,

	ERRMODE_PHPERR,
	ERRMODE_EXCEPT,
	ERRMODE_INTERN
};

typedef int (*jsonreader_read_t)  (jsonreader_object *obj, zval *retval);
typedef int (*jsonreader_write_t) (jsonreader_object *obj, zval *newval);

typedef struct _jsonreader_prop_handler {
	jsonreader_read_t   read_func;
	jsonreader_write_t  write_func;
} jsonreader_prop_handler;

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
