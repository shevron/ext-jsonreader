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
  | Author: Shahar Evron, shahar@prematureoptimization.org               |
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

static zend_object_handlers  jsonreader_obj_handlers;
static zend_class_entry     *jsonreader_ce;

typedef struct _jsonreader_object { 
	zend_object   std;
	php_stream   *stream;
	vktor_parser *parser;
	zend_bool     close_stream;
} jsonreader_object;

/* {{{ jsonreader_object_free_storage 
 * 
 * C-level object destructor for JSONReader objects
 */
static void 
jsonreader_object_free_storage(void *object TSRMLS_DC) 
{
	jsonreader_object *intern = (jsonreader_object *) object;

	zend_object_std_dtor(&intern->std TSRMLS_CC);

	if (intern->parser) {
		vktor_parser_free(intern->parser);
	}

	if (intern->stream && intern->close_stream) {
		php_stream_close(intern->stream);
	}

	efree(object);
}
/* }}} */

/* {{{ jsonreader_object_new 
 * 
 * C-level constructor of JSONReader objects. Does not initialize the vktor parser - 
 * this will be initialized when needed, by calling jsonreader_reset().
 */
static zend_object_value 
jsonreader_object_new(zend_class_entry *ce TSRMLS_DC) 
{
	zend_object_value  retval;
	jsonreader_object *intern;

	intern = ecalloc(1, sizeof(jsonreader_object));
	zend_object_std_init(&(intern->std), ce TSRMLS_CC);
	zend_hash_copy(intern->std.properties, &ce->default_properties, 
		(copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));

	retval.handle = zend_objects_store_put(intern, 
		(zend_objects_store_dtor_t) zend_objects_destroy_object, 
		(zend_objects_free_object_storage_t) jsonreader_object_free_storage, 
		NULL TSRMLS_CC);

	retval.handlers = &jsonreader_obj_handlers;

	return retval;
}
/* }}} */

/* {{{ jsonreader_reset 
 *
 * Initialize or reset an internal jsonreader object struct. Will close & free
 * any stream opened by the reader, and initialize the associated vktor parser 
 * (and free the old parser, if exists)
 */
static void 
jsonreader_reset(jsonreader_object *obj TSRMLS_DC)
{
	if (obj->parser) {
		vktor_parser_free(obj->parser);
	}
	obj->parser = vktor_parser_init(JSONREADER_G(max_nesting_level));

	if (obj->stream) {
		php_stream_close(obj->stream);
	}
}
/* }}} */

/* {{{ proto boolean JSONReader::open(mixed URI)

   Opens the URI (any valid PHP stream URI) that JSONReader will open to read
   from. Can accept either a URI as a string, or an already-open stream 
   resource.
*/
PHP_METHOD(jsonreader, open)
{
	zval               *object, *arg;
	jsonreader_object  *intern;
	php_stream         *tmp_stream;
	int                 options = ENFORCE_SAFE_MODE | REPORT_ERRORS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &arg) == FAILURE) {
		return;
	}

	object = getThis();
	intern = (jsonreader_object *) zend_object_store_get_object(object TSRMLS_CC);

	switch(Z_TYPE_P(arg)) {
		case IS_STRING:
			tmp_stream = php_stream_open_wrapper(Z_STRVAL_P(arg), "r", options, NULL);
			intern->close_stream = true;
			break;

		case IS_RESOURCE:
			php_stream_from_zval(tmp_stream, &arg);
			intern->close_stream = false;
			break;

		default:
			php_error_docref(NULL TSRMLS_CC, E_ERROR, 
				"argument is expected to be a resource of type stream or a URI string, %s given",
				zend_zval_type_name(arg));
			RETURN_FALSE;
			break;

	}

	if (intern->stream) {
		jsonreader_reset(intern TSRMLS_CC);
	}

	intern->stream = tmp_stream;

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto boolean JSONReader::close()

   Close the currently open JSON stream and free related resources
*/
PHP_METHOD(jsonreader, close)
{
	zval              *object;
	jsonreader_object *intern;

	object = getThis();
	intern = (jsonreader_object *) zend_object_store_get_object(object TSRMLS_CC);

	// Close stream, if open
	if (intern->stream) {
		php_stream_close(intern->stream);
		intern->stream = NULL;
	}

	// Free parser, if created
	if (intern->parser) {
		vktor_parser_free(intern->parser);
		intern->parser = NULL;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ ARG_INFO */
ZEND_BEGIN_ARG_INFO(arginfo_jsonreader_open, 0)
	ZEND_ARG_INFO(0, URI)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_jsonreader_close, 0)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ zend_function_entry jsonreader_class_methods */
static const zend_function_entry jsonreader_class_methods[] = {
	PHP_ME(jsonreader, open, arginfo_jsonreader_open, ZEND_ACC_PUBLIC)
	PHP_ME(jsonreader, close, arginfo_jsonreader_close, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ declare_jsonreader_class_entry
 * 
 */
static void 
declare_jsonreader_class_entry(TSRMLS_D)
{
	zend_class_entry ce;
	
	memcpy(&jsonreader_obj_handlers, zend_get_std_object_handlers(), 
		sizeof(zend_object_handlers));
	
	INIT_CLASS_ENTRY(ce, "JSONReader", jsonreader_class_methods);
	ce.create_object = jsonreader_object_new;

	jsonreader_ce = zend_register_internal_class(&ce TSRMLS_CC);
}
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

/* {{{ PHP_GINIT_FUNCTION(jsonreader)
 */
static PHP_GINIT_FUNCTION(jsonreader)
{
	jsonreader_globals->max_nesting_level = 64;
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(jsonreader)
{
	REGISTER_INI_ENTRIES();
	declare_jsonreader_class_entry(TSRMLS_C);
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

/* {{{ jsonreader_module_entry
 */
zend_module_entry jsonreader_module_entry = {
	STANDARD_MODULE_HEADER,
	"JSONReader",
	NULL, //jsonreader_functions,
	PHP_MINIT(jsonreader),
	PHP_MSHUTDOWN(jsonreader),
	NULL,
	NULL,
	PHP_MINFO(jsonreader),
	"0.1",
	PHP_MODULE_GLOBALS(jsonreader),
	PHP_GINIT(jsonreader),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
