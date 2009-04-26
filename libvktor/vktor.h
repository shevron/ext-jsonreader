/*
 * vktor JSON pull-parser library
 * 
 * Copyright (c) 2009 Shahar Evron
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file vktor.h
 * 
 * vktor header file - defines the external API and types available to vktor
 * users. 
 */

#ifndef _VKTOR_H

/**
 * Parser struct - this is the main object used by the user to parse a JSON 
 * stream. This opaque structure is defined internally in vktor.c.
 */
typedef struct _vktor_parser_struct vktor_parser;

/* type definitions */

/**
 * @defgroup external External API
 * @{
 */

/**
 * @enum vktor_token
 * 
 * JSON token types
 * 
 * Whenever a token is encountered during the parsing process, calling 
 * vktor_get_token_type() will return one of these values to indicate the token
 * type.
 */
typedef enum {
	VKTOR_T_NONE         =  0,
	VKTOR_T_NULL         =  1,
	VKTOR_T_FALSE        =  1 << 1,
	VKTOR_T_TRUE         =  1 << 2,
	VKTOR_T_INT          =  1 << 3,
	VKTOR_T_FLOAT        =  1 << 4,
	VKTOR_T_STRING       =  1 << 5,
	VKTOR_T_ARRAY_START  =  1 << 6,
	VKTOR_T_ARRAY_END    =  1 << 7,
	VKTOR_T_OBJECT_START =  1 << 8,
	VKTOR_T_OBJECT_KEY   =  1 << 9,
	VKTOR_T_OBJECT_END   =  1 << 10,
} vktor_token;

/**
 * @enum vktor_struct
 * 
 * Possible JSON struct types (array or object)
 */
typedef enum {
	VKTOR_STRUCT_NONE,  /**< No struct */
	VKTOR_STRUCT_ARRAY, /**< Array */
	VKTOR_STRUCT_OBJECT /**< Object (AKA map, associative array) */
} vktor_struct; 

/**
 * @enum vktor_status
 * 
 * Possible vktor parser status codes
 */
typedef enum {
	VKTOR_ERROR,     /**< An error has occured */
	VKTOR_OK,        /**< Everything is OK */
	VKTOR_MORE_DATA, /**< More data is required in order to continue parsing */
	VKTOR_COMPLETE   /**< Parsing is complete, no further data is expected */
} vktor_status;

/**
 * @enum vktor_errcode
 * 
 * Possible error codes used in vktor_error->code 
 */
typedef enum {
	VKTOR_ERR_NONE,             /**< no error */
	VKTOR_ERR_OUT_OF_MEMORY,    /**< can't allocate memory */
	VKTOR_ERR_UNEXPECTED_INPUT, /**< unexpected characters in input buffer */
	VKTOR_ERR_INCOMPLETE_DATA,  /**< can't finish parsing without more data */
	VKTOR_ERR_NO_VALUE,         /**< trying to read non-existing value */
	VKTOR_ERR_OUT_OF_RANGE,     /**< long or double value is out of range */
	VKTOR_ERR_MAX_NEST,         /**< maximal nesting level reached */
	VKTOR_ERR_INTERNAL_ERR      /**< internal parser error */
} vktor_errcode;

/**
 * Error structure, signifying the error code and error message
 * 
 * Error structs must be freed using vktor_error_free()
 */
typedef struct _vktor_error_struct {
	vktor_errcode  code;    /**< error code */
	char          *message; /**< error message */
} vktor_error;

/* function prototypes */

/**
 * @brief Initialize a new parser 
 * 
 * Initialize and return a new parser struct. Will return NULL if memory can't 
 * be allocated.
 * 
 * @param [in] max_nest maximal nesting level
 * 
 * @return a newly allocated parser
 */
vktor_parser* vktor_parser_init(int max_nest);

/**
 * @brief Free a parser and any associated memory
 * 
 * Free a parser and any associated memory structures, including any linked
 * buffers
 * 
 * @param [in,out] parser parser struct to free
 */
void vktor_parser_free(vktor_parser *parser);

/**
 * @brief Free an error struct
 * 
 * Free an error struct
 * 
 * @param [in,out] err Error struct to free
 */
void vktor_error_free(vktor_error *err);

/**
 * @brief Feed the parser's internal buffer with more JSON data
 * 
 * Feed the parser's internal buffer with more JSON data, to be used later when 
 * parsing. This function should be called before starting to parse at least
 * once, and again whenever new data is available and the VKTOR_MORE_DATA 
 * status is returned from vktor_parse().
 * 
 * @param [in] parser   parser object
 * @param [in] text     text to add to buffer
 * @param [in] text_len length of text to add to buffer
 * @param [in,out] err  pointer to an unallocated error struct to return any 
 *                      errors, or NULL if there is no need for error handling
 * 
 * @return vktor status code 
 *  - VKTOR_OK on success 
 *  - VKTOR_ERROR otherwise
 */
vktor_status vktor_feed(vktor_parser *parser, char *text, long text_len, 
                        vktor_error **err);

/**
 * @brief Parse some JSON text and return on the next token
 * 
 * Parse the text buffer until the next JSON token is encountered
 * 
 * In case of error, if error is not NULL, it will be populated with error 
 * information, and VKTOR_ERROR will be returned
 * 
 * @param [in,out] parser The parser object to work with
 * @param [out]    error  A vktor_error pointer pointer, or NULL
 * 
 * @return Status code:
 *  - VKTOR_OK        if a token was encountered
 *  - VKTOR_ERROR     if an error has occured
 *  - VKTOR_MORE_DATA if we need more data in order to continue parsing
 *  - VKTOR_COMPLETE  if parsing is complete and no further data is expected
 */
vktor_status vktor_parse(vktor_parser *parser, vktor_error **error);
		  
/**
 * @brief Get the current token type
 * 
 * Get the type of the current token pointed to by the parser
 * 
 * @param [in] parser Parser object
 * 
 * @return Token type (one of the VKTOR_T_* tokens)
 */
vktor_token vktor_get_token_type(vktor_parser *parser);

/**
 * @brief Get the current nesting depth
 * 
 * Get the current array/object nesting depth of the current token the parser
 * is pointing to
 * 
 * @param [in] parser Parser object
 * 
 * @return nesting level - 0 means top level
 */
int vktor_get_depth(vktor_parser *parser);

/**
 * @brief Get the current struct type
 * 
 * Get the struct type (object, array or none) containing the current token
 * pointed to by the parser
 * 
 * @param [in] parser Parser object
 * 
 * @return A vktor_struct value or VKTOR_STRUCT_NONE if we are in the top 
 *   level
 */
vktor_struct vktor_get_current_struct(vktor_parser *parser);

/**
 * @brief Get the token value as a long integer
 * 
 * Get the value of the current token as a long integer. Suitable for reading
 * the value of VKTOR_T_INT tokens, but can also be used to get the integer 
 * value of VKTOR_T_FLOAT tokens and even any numeric prefix of a VKTOR_T_STRING
 * token. 
 * 
 * If the value of a number token is larger than the system's maximal long, 
 * 0 is returned and error will indicate overflow. In such cases, 
 * vktor_get_value_string() should be used to get the value as a string.
 * 
 * @param [in]  parser Parser object
 * @param [out] error  Error object pointer pointer or null
 * 
 * @return The numeric value of the current token as a long int, 
 * @retval 0 in case of error (although 0 might also be normal, so check the 
 *         value of error)
 */
long vktor_get_value_long(vktor_parser *parser, vktor_error **error);

/**
 * @brief Get the token value as a double
 * 
 * Get the value of the current token as a double precision floating point 
 * number. Suitable for reading the value of VKTOR_T_FLOAT tokens.
 * 
 * If the value of a number token is larger than the system's HUGE_VAL 0 is 
 * returned and error will indicate overflow. In such cases, 
 * vktor_get_value_string() should be used to get the value as a string.
 * 
 * @param [in]  parser Parser object
 * @param [out] error  Error object pointer pointer or null
 * 
 * @return The numeric value of the current token as a double 
 * @retval 0 in case of error (although 0 might also be normal, so check the 
 *         value of error)
 */
double vktor_get_value_double(vktor_parser *parser, vktor_error **error);

/**
 * @brief Get the value of the token as a string
 * 
 * Get the value of the current token as a string, as well as the length of the
 * token. Suitable for getting the value of a VKTOR_T_STRING token, but also 
 * for reading numeric values as a string. 
 * 
 * Note that the string pointer populated into val is owned by the parser and 
 * should not be freed by the user.
 * 
 * @param [in]  parser Parser object
 * @param [out] val    Pointer-pointer to be populated with the value
 * @param [out] error  Error object pointer pointer or NULL
 * 
 * @return The length of the string
 * @retval 0 in case of error (although 0 might also be normal, so check the 
 *         value of error)
 */
int vktor_get_value_str(vktor_parser *parser, char **val, vktor_error **error);

/**
 * @brief Get the value of the token as a string
 * 
 * Similar to vktor_get_value_str(), only this function will provide a copy of 
 * the string, which will need to be freed by the user when it is no longer 
 * used. 
 * 
 * @param [in]  parser Parser object
 * @param [out] val    Pointer-pointer to be populated with the value
 * @param [out] error  Error object pointer pointer or NULL
 * 
 * @return The length of the string
 * @retval 0 in case of error (although 0 might also be normal, so check the 
 *         value of error)
 */
int vktor_get_value_str_copy(vktor_parser *parser, char **val, vktor_error **error);

/** @} */ // end of external API

#define _VKTOR_H
#endif /* VKTOR_H */
