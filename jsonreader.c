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
#include "zend_exceptions.h"

#include "libvktor/vktor.h"

ZEND_DECLARE_MODULE_GLOBALS(jsonreader)

static zend_object_handlers  jsonreader_obj_handlers;
zend_class_entry            *jsonreader_ce;
static zend_class_entry     *jsonreader_exception_ce;
static HashTable jsonreader_prop_handlers;

/* {{{ Memory management functions wrapping emalloc etc. */

/* {{{ jsr_malloc
   Wrapper for PHP's emalloc, passed to libvktor as malloc alternative */
static void *jsr_malloc(size_t size)
{
	return ecalloc(1, size);
}
/* }}} */

/* {{{ jsr_realloc
   Wrapper for PHP's erealloc, passed to libvktor as realloc alternative */
static void *jsr_realloc(void *ptr, size_t size)
{
	return erealloc(ptr, size);
}
/* }}} */

/* {{{ jsr_free 
   Wrapper for PHP's efree, passed to libvktor as free alternative */
static void jsr_free(void *ptr)
{
	efree(ptr);
}
/* }}} */

/* }}} */

/* {{{ jsonreader_handle_error
   Handle a parser error - for now generate an E_WARNING, in the future this might
   also do things like throw an exception or use an internal error handler */
static void jsonreader_handle_error(vktor_error *err, jsonreader_object *obj)
{
	switch(obj->errmode) {
		case ERRMODE_PHPERR:
			php_error_docref(NULL, E_WARNING, "parser error [#%d]: %s", err->code, err->message);
			break;

		case ERRMODE_EXCEPT:
			zend_throw_exception_ex(jsonreader_exception_ce, err->code, "%s", err->message);
			break;

		default: // For now emit a PHP WARNING
			php_error_docref(NULL, E_WARNING, "parser error [#%d]: %s", err->code, err->message);
			break;
	}

	vktor_error_free(err);
}
/* }}} */

/* {{{ jsonreader_read_na 
   Called when a user tries to read a write-only property of a JSONReader object */
static int jsonreader_read_na(jsonreader_object *obj, zval *retval)
{
	php_error_docref(NULL, E_ERROR, "trying to read a write-only property");
	return FAILURE;
}
/* }}} */

/* {{{ jsonreader_write_na 
   Called when a user tries to write to a read-only property of a JSONReader object */
static int jsonreader_write_na(jsonreader_object *obj, zval *newval)
{
	php_error_docref(NULL, E_ERROR, "trying to modify a read-only property");
	return FAILURE;
}
/* }}} */

/* {{{ jsonreader_register_prop_handler 
   Register a read/write handler for a specific property of JSONReader objects */
static void jsonreader_register_prop_handler(char *name, jsonreader_read_t read_func, jsonreader_write_t write_func)
{
	jsonreader_prop_handler jph;

	jph.read_func  = read_func  ? read_func  : jsonreader_read_na;
	jph.write_func = write_func ? write_func : jsonreader_write_na;

	zend_hash_str_add_mem(&jsonreader_prop_handlers, name, strlen(name), &jph, sizeof(jsonreader_prop_handler));
}
/* }}} */

/* {{{ jsonreader_read_property
   Property read handler */
static zval *jsonreader_read_property(zval *object, zval *member, int type, void **cache_slot, zval *rv)
{
	zval                     retval;
	jsonreader_prop_handler *jph;
	FETCH_JSON_OBJECT_FROM_ZV(object)

	if (Z_TYPE_P(member) != IS_STRING) {
		goto std_h;
	}

	if ((jph = (jsonreader_prop_handler *)zend_hash_find_ptr(&jsonreader_prop_handlers, Z_STR_P(member))) != NULL) {
		if (jph->read_func(intern, &retval) == SUCCESS) {
			ZVAL_COPY_VALUE(rv, &retval);
			return rv;
		} else {
			return &EG(uninitialized_zval);
		}
	} else {
std_h:
		ZVAL_COPY_VALUE(rv, std_object_handlers.read_property(object, member, type, cache_slot, rv));
		return rv;
	}
}
/* }}} */

/* {{{ jsonreader_write_property */
static void jsonreader_write_property(zval *object, zval *member, zval *value, void **cache_slot)
{
	FETCH_JSON_OBJECT_FROM_ZV(object)
	jsonreader_prop_handler *jph;

	if (Z_TYPE_P(member) != IS_STRING) {
		goto std_h;
	}

	if ((jph = (jsonreader_prop_handler *)zend_hash_find_ptr(&jsonreader_prop_handlers, Z_STR_P(member))) != NULL) {
		jph->write_func(intern, value);
	} else {
std_h:
		std_object_handlers.write_property(object, member, value, cache_slot);
	}

}
/* }}} */

/* {{{ jsonreader_get_token_type
   Get the type of the current token */
static int jsonreader_get_token_type(jsonreader_object *obj, zval *retval)
{
	vktor_token token;

	if (! obj->parser) {
		ZVAL_NULL(retval);
	} else {
		token = vktor_get_token_type(obj->parser);
		if (token == VKTOR_T_NONE) {
			ZVAL_NULL(retval);
		} else {
			ZVAL_LONG(retval, token);
		}
	}

	return SUCCESS;
}
/* }}} */

/* {{{ jsonreader_get_token_value
   Get the value of the current token */
static int jsonreader_get_token_value(jsonreader_object *obj, zval *retval)
{
	vktor_token  t_type;
	vktor_error *err = NULL;

	if (! obj->parser) {
		ZVAL_NULL(retval);
	} else {
		t_type = vktor_get_token_type(obj->parser);
		switch(t_type) {
			case VKTOR_T_NONE:
			case VKTOR_T_NULL:
			case VKTOR_T_ARRAY_START:
			case VKTOR_T_ARRAY_END:
			case VKTOR_T_OBJECT_START:
			case VKTOR_T_OBJECT_END:
				ZVAL_NULL(retval);
				break;

			case VKTOR_T_FALSE:
				ZVAL_FALSE(retval);
				break;
			case VKTOR_T_TRUE:
				ZVAL_TRUE(retval);
				break;

			case VKTOR_T_OBJECT_KEY:
			case VKTOR_T_STRING: {
				char *strval;
				int   strlen;

				strlen = vktor_get_value_str(obj->parser, &strval, &err);
				if (err != NULL) {
					ZVAL_NULL(retval);
					jsonreader_handle_error(err, obj);
					return FAILURE;
				}

				ZVAL_STRINGL(retval, strval, strlen);
				break;
			}
			
			case VKTOR_T_INT:
				ZVAL_LONG(retval, vktor_get_value_long(obj->parser, &err));
				if (err != NULL) {
					ZVAL_NULL(retval);
					jsonreader_handle_error(err, obj);
					return FAILURE;
				}
				break;

			case VKTOR_T_FLOAT:
				ZVAL_DOUBLE(retval, vktor_get_value_double(obj->parser, &err));
				if (err != NULL) {
					ZVAL_NULL(retval);
					jsonreader_handle_error(err, obj);
					return FAILURE;
				}
				break;

			default: /* should not happen */
				php_error_docref(NULL, E_ERROR, "internal error: unkown token type %d", t_type);
				return FAILURE;
				break;
		}
	}

	return SUCCESS;
}
/* }}} */

/* {{{ jsonreader_get_current_struct 
   Get the type of the current JSON struct we are in (object, array or none) */
static int jsonreader_get_current_struct(jsonreader_object *obj, zval *retval)
{
	vktor_struct cs;

	if (! obj->parser) {
		ZVAL_NULL(retval);
	} else {
		cs = vktor_get_current_struct(obj->parser);
		if (cs == VKTOR_STRUCT_NONE) {
			ZVAL_NULL(retval);
		} else {
			ZVAL_LONG(retval, cs);
		}
	}

	return SUCCESS;
}
/* }}} */

/* {{{ jsonreader_get_current_depth 
   Get the current nesting level */
static int jsonreader_get_current_depth(jsonreader_object *obj, zval *retval)
{
	int depth;

	if (!obj->parser) {
		ZVAL_NULL(retval);
	} else {
		depth = vktor_get_depth(obj->parser);
		ZVAL_LONG(retval, depth);
	}

	return SUCCESS;
}
/* }}} */

/* {{{ jsonreader_object_free_storage 
   C-level object destructor for JSONReader objects */
static void jsonreader_object_free_storage(zend_object *object)
{
	FETCH_JSON_OBJECT_FROM_OBJ(object)

	zend_object_std_dtor(&intern->std);

	if (intern->parser) {
		vktor_parser_free(intern->parser);
	}

	if (intern->stream && !intern->internal_stream) {
		php_stream_close(intern->stream);
	}
}
/* }}} */

/* {{{ jsonreader_object_new 
   C-level constructor of JSONReader objects. Does not initialize the vktor 
   parser - this will be initialized when needed, by calling jsonreader_init() */
static zend_object *jsonreader_object_new(zend_class_entry *ce)
{
	jsonreader_object *intern;

	intern = ecalloc(1, sizeof(jsonreader_object) + zend_object_properties_size(ce));
	intern->max_depth   = JSONREADER_G(max_depth);
	intern->read_buffer = JSONREADER_G(read_buffer);
	intern->errmode     = ERRMODE_PHPERR;

	zend_object_std_init(&intern->std, ce);

	object_properties_init(&intern->std, ce);

	intern->std.handlers = &jsonreader_obj_handlers;

	return &intern->std;
}
/* }}} */

/* {{{ jsonreader_read_more_data 
   Read more data from the stream and pass it to the parser  */
static int jsonreader_read_more_data(jsonreader_object *obj)
{
	char         *buffer;
	int           read;
	vktor_status  status;
	vktor_error  *err;
	
	buffer = ecalloc(1, obj->read_buffer);

	read = php_stream_read(obj->stream, buffer, obj->read_buffer);
	if (read <= 0) {
		/* done reading or error */
		php_error_docref(NULL, E_WARNING, "JSON stream ended while expecting more data");
		efree(buffer);
		return FAILURE;
	}

	status = vktor_feed(obj->parser, buffer, read, 1, &err);

	if (status == VKTOR_ERROR) {
		jsonreader_handle_error(err, obj);
		return FAILURE;
	}

	return SUCCESS;
}
/* }}} */

/* {{{ jsonreader_read
   Read the next token from the JSON stream */
static int jsonreader_read(jsonreader_object *obj)
{
	vktor_status  status = VKTOR_OK;
	vktor_error  *err;
	int           retval;

	do {
		status = vktor_parse(obj->parser, &err);

		switch(status) {
			case VKTOR_OK:
				retval = SUCCESS;
				break;

			case VKTOR_COMPLETE:
				retval = FAILURE; /* done, not really a failure */
				break;

			case VKTOR_ERROR:
				jsonreader_handle_error(err, obj);
				return FAILURE;

			case VKTOR_MORE_DATA:
				if (jsonreader_read_more_data(obj) == FAILURE) {
					retval = FAILURE;
					status = VKTOR_ERROR;
				}
				break;

			default:
				/* should not happen! */
				php_error_docref(NULL, E_ERROR, "invalid status from internal JSON parser");
				retval = FAILURE;
				break;
		}

	} while (status == VKTOR_MORE_DATA);

	return retval; 
}
/* }}} */

/* {{{ jsonreader_set_attribute 
   set an attribute of the JSONReader object */
static void jsonreader_set_attribute(jsonreader_object *obj, zend_ulong attr_key, zval *attr_value)
{
	long lval = Z_LVAL_P(attr_value);

	switch(attr_key) {
		case ATTR_MAX_DEPTH:
			if (lval < 1) {
				php_error_docref(NULL, E_WARNING, "maximal nesting level must be more than 0, %ld given", lval);
			} else {
				obj->max_depth = lval;
			}
			break;

		case ATTR_READ_BUFF:
			if (lval < 1) {
				php_error_docref(NULL, E_WARNING, "read buffer size must be more than 0, %ld given", lval);
			} else {
				obj->read_buffer = lval;
			}
			break;

		case ATTR_ERRMODE:
			switch(lval) {
				case ERRMODE_PHPERR:
				case ERRMODE_EXCEPT:
				case ERRMODE_INTERN:
					obj->errmode = (int) lval;
					break;

				default:
					php_error_docref(NULL, E_WARNING, "invalid error handler attribute value: %ld", lval);
					break;
			}
			break;
	}
}

/* {{{ proto void JSONReader::__construct([array options])
   Create a new JSONReader object, potentially setting some local attributes */
PHP_METHOD(jsonreader, __construct)
{
	zval *options = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|a", &options) == FAILURE) {
		//ZVAL_NULL(getThis());
		return;
	}

	/* got attributes - set them */
	if (options) {
		zval               *val;
		zend_ulong          h;
		zend_string        *key;
		FETCH_JSON_OBJECT

		ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(options), h, key, val)
			if (!key) {
				jsonreader_set_attribute(intern, h, val);
			}
		ZEND_HASH_FOREACH_END();
	}
}
/* }}} */

/* {{{ proto boolean JSONReader::open(mixed URI)
   Opens the URI (any valid PHP stream URI) that JSONReader will read
   from. Can accept either a URI as a string, or an already-open stream 
   resource. Returns TRUE on success or FALSE on failure. */
PHP_METHOD(jsonreader, open)
{
	zval               *object, *arg;
	php_stream         *tmp_stream;

	FETCH_JSON_OBJECT

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &arg) == FAILURE) {
		return;
	}

	switch(Z_TYPE_P(arg)) {
		case IS_STRING:
			tmp_stream = php_stream_open_wrapper(Z_STRVAL_P(arg), "r", REPORT_ERRORS, NULL);
			if (tmp_stream) {
				intern->stream       = tmp_stream;
			}
			break;

		case IS_RESOURCE:
			php_stream_from_zval(tmp_stream, arg);
			intern->internal_stream = tmp_stream->abstract;
			intern->stream          = tmp_stream;
			break;

		default:
			php_error_docref(NULL, E_WARNING, "argument is expected to be a resource of type stream or a string, %s given", zend_zval_type_name(arg));
			RETURN_FALSE;
	}

	intern->parser = vktor_parser_init(intern->max_depth);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto boolean JSONReader::close()
   Close the currently open JSON stream and free related resources */
PHP_METHOD(jsonreader, close)
{
	zval              *object;
	FETCH_JSON_OBJECT

	/* Close stream, if open */
	if (intern->stream && !intern->stream->abstract) {
		php_stream_close(intern->stream);
		intern->stream          = NULL;
	}

	/* Free parser, if created */
	if (intern->parser) {
		vktor_parser_free(intern->parser);
		intern->parser = NULL;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto boolean JSONReader::read() 
   Read the next token from the JSON stream. Retuns TRUE as long as something is
   read, or FALSE when there is nothing left to read, or when an error occured. */
PHP_METHOD(jsonreader, read)
{
	zval              *object;

	RETVAL_TRUE;

	FETCH_JSON_OBJECT

	if (!intern->stream || (intern->internal_stream && intern->stream->abstract != intern->internal_stream)) {
		php_error_docref(NULL, E_WARNING, "trying to read but no stream was opened or stream was closed");
		RETURN_FALSE;
	}

	if (jsonreader_read(intern TSRMLS_CC) != SUCCESS) {
		RETVAL_FALSE;
	}
}
/* }}} */

/* {{{ ARG_INFO */
ZEND_BEGIN_ARG_INFO(arginfo_jsonreader___construct, 0)
	ZEND_ARG_INFO(0, attributes)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_jsonreader_open, 0)
	ZEND_ARG_INFO(0, URI)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_jsonreader_close, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_jsonreader_read, 0)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ zend_function_entry jsonreader_class_methods */
static const zend_function_entry jsonreader_class_methods[] = {
	PHP_ME(jsonreader, __construct, arginfo_jsonreader___construct, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
	PHP_ME(jsonreader, open,  arginfo_jsonreader_open,  ZEND_ACC_PUBLIC)
	PHP_ME(jsonreader, close, arginfo_jsonreader_close, ZEND_ACC_PUBLIC)
	PHP_ME(jsonreader, read,  arginfo_jsonreader_read,  ZEND_ACC_PUBLIC)
	PHP_FE_END
};
/* }}} */

#ifdef COMPILE_DL_JSONREADER
ZEND_GET_MODULE(jsonreader)
#endif

/* {{{ PHP_INI */
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("jsonreader.max_depth", XSTRINGIFY(JSONREADER_DEFAULT_MAX_DEPTH), PHP_INI_ALL, OnUpdateLongGEZero, max_depth, zend_jsonreader_globals, jsonreader_globals)
    STD_PHP_INI_ENTRY("jsonreader.read_buffer", XSTRINGIFY(JSONREADER_DEFAULT_MAX_READ_BUFFER), PHP_INI_ALL, OnUpdateLongGEZero, read_buffer, zend_jsonreader_globals, jsonreader_globals)
PHP_INI_END()
/* }}} */

/* {{{ PHP_GINIT_FUNCTION */
PHP_GINIT_FUNCTION(jsonreader)
{
	jsonreader_globals->max_depth  = JSONREADER_DEFAULT_MAX_DEPTH;
	jsonreader_globals->read_buffer= JSONREADER_DEFAULT_MAX_READ_BUFFER;
}
/* }}} */

static void json_prop_handlers_dtor(zval *el)
{
	pefree(Z_PTR_P(el), 1);
}

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(jsonreader)
{
	zend_class_entry ce;

	REGISTER_INI_ENTRIES();
	zend_hash_init(&jsonreader_prop_handlers, 8, NULL, json_prop_handlers_dtor, 1);

	memcpy(&jsonreader_obj_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	jsonreader_obj_handlers.read_property  = jsonreader_read_property;
	jsonreader_obj_handlers.write_property = jsonreader_write_property;
	jsonreader_obj_handlers.offset         = XtOffsetOf(struct _jsonreader_object, std);
	jsonreader_obj_handlers.free_obj       = jsonreader_object_free_storage;

	INIT_CLASS_ENTRY(ce, "JSONReader", jsonreader_class_methods);
	ce.create_object = jsonreader_object_new;

	jsonreader_ce = zend_register_internal_class(&ce);

	JSONREADER_REG_CLASS_CONST_L("ATTR_MAX_DEPTH", ATTR_MAX_DEPTH);
	JSONREADER_REG_CLASS_CONST_L("ATTR_READ_BUFF", ATTR_READ_BUFF);
	JSONREADER_REG_CLASS_CONST_L("ATTR_ERRMODE",   ATTR_ERRMODE);
	JSONREADER_REG_CLASS_CONST_L("ERRMODE_PHPERR", ERRMODE_PHPERR);
	JSONREADER_REG_CLASS_CONST_L("ERRMODE_EXCEPT", ERRMODE_EXCEPT);
	JSONREADER_REG_CLASS_CONST_L("ERRMODE_INTERN", ERRMODE_INTERN);

	JSONREADER_REG_CLASS_CONST_L("NULL",         VKTOR_T_NULL);
	JSONREADER_REG_CLASS_CONST_L("FALSE",        VKTOR_T_FALSE);
	JSONREADER_REG_CLASS_CONST_L("TRUE",         VKTOR_T_TRUE);
	JSONREADER_REG_CLASS_CONST_L("BOOLEAN",      VKTOR_T_FALSE | VKTOR_T_TRUE);
	JSONREADER_REG_CLASS_CONST_L("INT",          VKTOR_T_INT);
	JSONREADER_REG_CLASS_CONST_L("FLOAT",        VKTOR_T_FLOAT);
	JSONREADER_REG_CLASS_CONST_L("NUMBER",       VKTOR_T_INT | VKTOR_T_FLOAT);
	JSONREADER_REG_CLASS_CONST_L("STRING",       VKTOR_T_STRING);
	JSONREADER_REG_CLASS_CONST_L("VALUE",        JSONREADER_VALUE_TOKEN);
	JSONREADER_REG_CLASS_CONST_L("ARRAY_START",  VKTOR_T_ARRAY_START);
	JSONREADER_REG_CLASS_CONST_L("ARRAY_END",    VKTOR_T_ARRAY_END);
	JSONREADER_REG_CLASS_CONST_L("OBJECT_START", VKTOR_T_OBJECT_START);
	JSONREADER_REG_CLASS_CONST_L("OBJECT_KEY",   VKTOR_T_OBJECT_KEY);
	JSONREADER_REG_CLASS_CONST_L("OBJECT_END",   VKTOR_T_OBJECT_END);

	JSONREADER_REG_CLASS_CONST_L("ARRAY",        VKTOR_STRUCT_ARRAY);
	JSONREADER_REG_CLASS_CONST_L("OBJECT",       VKTOR_STRUCT_OBJECT);

	jsonreader_register_prop_handler("tokenType", jsonreader_get_token_type, NULL);
	jsonreader_register_prop_handler("value", jsonreader_get_token_value, NULL);
	jsonreader_register_prop_handler("currentStruct", jsonreader_get_current_struct, NULL);
	jsonreader_register_prop_handler("currentDepth", jsonreader_get_current_depth, NULL);

	INIT_CLASS_ENTRY(ce, "JSONReaderException", NULL);
	jsonreader_exception_ce = zend_register_internal_class_ex(&ce, zend_exception_get_default());

	vktor_set_memory_handlers(jsr_malloc, jsr_realloc, jsr_free);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(jsonreader)
{
	UNREGISTER_INI_ENTRIES();
	zend_hash_destroy(&jsonreader_prop_handlers);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(jsonreader)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "jsonreader support", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

/* {{{ jsonreader_module_entry */
zend_module_entry jsonreader_module_entry = {
	STANDARD_MODULE_HEADER,
	"JSONReader",
	NULL, 
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
