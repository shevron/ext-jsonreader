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
 * @file vktor.c
 * 
 * Main vktor library file. Defines the external API of vktor as well as some 
 * internal static functions, data types and macros.
 */

/**
 * @defgroup internal Internal API
 * @internal
 * @{
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "vktor.h"
#include "vktor_unicode.h"

/**
 * Maximal error string length (mostly for internal use). 
 */
#define VKTOR_MAX_E_LEN 256

/**
 * Memory allocation chunk size used when reading strings. Make sure it is 
 * never below 4 which is the largent possible size of a Unicode character.
 */
#ifndef VKTOR_STR_MEMCHUNK
#define VKTOR_STR_MEMCHUNK 128
#endif

/**
 * Memory allocation chunk size used when reading numbers
 */
#ifndef VKTOR_NUM_MEMCHUNK
#define VKTOR_NUM_MEMCHUNK 32
#endif

/**
 * Convenience macro to check if we are at the end of a buffer
 */
#define eobuffer(b) (b->ptr >= b->size)

/**
 * Convenience macro to set an 'unexpected character' error
 */
#define set_error_unexpected_c(e, c)         \
	set_error(e, VKTOR_ERR_UNEXPECTED_INPUT, \
		"Unexpected character in input: '%c' (0x%02hhx)", c, c)

/**
 * A bitmask representing any 'value' token 
 */
#define VKTOR_VALUE_TOKEN VKTOR_T_NULL        | \
                          VKTOR_T_FALSE       | \
						  VKTOR_T_TRUE        | \
						  VKTOR_T_INT         | \
						  VKTOR_T_FLOAT       | \
						  VKTOR_T_STRING      | \
						  VKTOR_T_ARRAY_START | \
						  VKTOR_T_OBJECT_START

/**
 * Convenience macro to check if we are in a specific type of JSON struct
 */
#define nest_stack_in(p, c) (p->nest_stack[p->nest_ptr] == c)

/**
 * Convenience macro to easily set the expected next token map after a value
 * token, taking current struct struct (if any) into account.
 */
#define expect_next_value_token(p)              \
	switch(p->nest_stack[p->nest_ptr]) {        \
		case VKTOR_STRUCT_OBJECT:               \
			p->expected = VKTOR_C_COMMA |       \
							VKTOR_T_OBJECT_END; \
			break;                              \
												\
		case VKTOR_STRUCT_ARRAY:                \
			p->expected = VKTOR_C_COMMA |       \
							VKTOR_T_ARRAY_END;  \
			break;                              \
												\
		default:                                \
			p->expected = VKTOR_T_NONE;         \
			break;                              \
	}

/**
 * Convenience macro to check, and reallocate if needed, the memory size for
 * reading a token
 */
#define check_reallocate_token_memory(cs)                              \
	if ((ptr + 5) >= maxlen) {                                         \
		maxlen = maxlen + cs;                                          \
		if ((token = realloc(token, maxlen * sizeof(char))) == NULL) { \
			set_error(error, VKTOR_ERR_OUT_OF_MEMORY,                  \
				"unable to allocate %d more bytes for string parsing", \
				cs);                                                   \
			return VKTOR_ERROR;                                        \
		}                                                              \
	}

/**
 * Buffer struct, containing some text to parse along with an internal pointer
 * and a link to the next buffer.
 * 
 * vktor internally holds text to be parsed as a linked list of buffers pushed
 * by the user, so no memory reallocations are required. Whenever a buffer is 
 * completely parsed, the parser will advance to the next buffer pointed by 
 * #next_buff and will free the previous buffer
 * 
 * This is done internally by the parser
 */
typedef struct _vktor_buffer_struct {
	char                        *text;      /**< buffer text */
	long                         size;      /**< buffer size */
	long                         ptr;       /**< internal buffer position */
	struct _vktor_buffer_struct *next_buff;	/**< pointer to the next buffer */
} vktor_buffer;

/**
 * Parser struct - this is the main object used by the user to parse a JSON 
 * stream. 
 */
struct _vktor_parser_struct {
	vktor_buffer   *buffer;       /**< the current buffer being parsed */
	vktor_buffer   *last_buffer;  /**< a pointer to the last buffer */
	vktor_token     token_type;   /**< current token type */
	void           *token_value;  /**< current token value, if any */
	int             token_size;   /**< current token value length, if any */
	char            token_resume; /**< current token is only half read */  
	long            expected;     /**< bitmask of possible expected tokens */
	vktor_struct   *nest_stack;   /**< array holding current nesting stack */
	int             nest_ptr;     /**< pointer to the current nesting level */
	int             max_nest;     /**< maximal nesting level */
	unsigned long   unicode_c;    /**< temp container for unicode characters */
};

/**
 * @enum vktor_specialchar
 * 
 * Special JSON characters - these are not returned to the user as tokens 
 * but still have a meaning when parsing JSON.
 */
typedef enum {
	VKTOR_C_COMMA   = 1 << 16, /**< ",", used as struct separator */
	VKTOR_C_COLON   = 1 << 17, /**< ":", used to separate object key:value */
	VKTOR_C_DOT     = 1 << 18, /**< ".", used in floating-point numbers */ 
	VKTOR_C_SIGNUM  = 1 << 19, /**< "+" or "-" used in numbers */
	VKTOR_C_EXP     = 1 << 20, /**< "e" or "E" used for number exponent */
	VKTOR_C_ESCAPED = 1 << 21, /**< An escaped character */
	VKTOR_C_UNIC1   = 1 << 22, /**< Unicode encoded character (1st byte) */
	VKTOR_C_UNIC2   = 1 << 23, /**< Unicode encoded character (2nd byte) */
	VKTOR_C_UNIC3   = 1 << 24, /**< Unicode encoded character (3rd byte) */
	VKTOR_C_UNIC4   = 1 << 25, /**< Unicode encoded character (4th byte) */
	VKTOR_C_UNIC_LS = 1 << 26, /**< Unicode low surrogate */
} vktor_specialchar;

/**
 * @brief Free a vktor_buffer struct
 * 
 * Free a vktor_buffer struct without following any next buffers in the chain. 
 * Call buffer_free_all() to free an entire chain of buffers.
 * 
 * @param[in,out] buffer the buffer to free
 */
static void 
buffer_free(vktor_buffer *buffer)
{
	assert(buffer != NULL);
	assert(buffer->text != NULL);
	
	free(buffer->text);
	free(buffer);
}

/**
 * @brief Free an entire linked list of vktor buffers
 * 
 * Free an entire linked list of vktor buffers. Will usually be called by 
 * vktor_parser_free() to free all buffers attached to a parser. 
 * 
 * @param[in,out] buffer the first buffer in the list to free
 */
static void 
buffer_free_all(vktor_buffer *buffer)
{
	vktor_buffer *next;
	
	while (buffer != NULL) {
		next = buffer->next_buff;
		buffer_free(buffer);
		buffer = next;
	}
}

/**
 * @brief Initialize and populate new error struct
 *
 * If eptr is NULL, will do nothing. Otherwise, will initialize a new error
 * struct with an error message and code, and set eptr to point to it. 
 * 
 * The error message is passed as an sprintf-style format and an arbitrary set
 * of parameters, just like sprintf() would be used. 
 * 
 * Used internally to pass error messages back to the user. 
 * 
 * @param [in,out] eptr error struct pointer-pointer to populate or NULL
 * @param [in]     code error code
 * @param [in]     msg  error message (sprintf-style format)
 */
static void 
set_error(vktor_error **eptr, vktor_errcode code, const char *msg, ...)
{
	vktor_error *err;
	
	if (eptr == NULL) {
		return;
	}
	
	if ((err = malloc(sizeof(vktor_error))) == NULL) {
		return;
	}
	
	err->code = code;
	err->message = malloc(VKTOR_MAX_E_LEN * sizeof(char));
	if (err->message != NULL) {
		va_list ap;
		      
		va_start(ap, msg);
		vsnprintf(err->message, VKTOR_MAX_E_LEN, msg, ap);
		va_end(ap);
	}
	
	*eptr = err;
}

/**
 * @brief Initialize a vktor buffer struct
 * 
 * Initialize a vktor buffer struct and set it's associated text and other 
 * properties
 * 
 * @param [in] text buffer contents
 * @param [in] text_len the length of the buffer
 * 
 * @return A newly-allocated buffer struct
 */
static vktor_buffer*
buffer_init(char *text, long text_len)
{
	vktor_buffer *buffer;
	
	if ((buffer = malloc(sizeof(vktor_buffer))) == NULL) {
		return NULL;
	}
	
	buffer->text      = text;
	buffer->size      = text_len;
	buffer->ptr       = 0;
	buffer->next_buff = NULL;
	
	return buffer;
}

/**
 * @brief Advance the parser to the next buffer
 * 
 * Advance the parser to the next buffer in the parser's buffer list. Called 
 * when the end of the current buffer is reached, and more data is required. 
 * 
 * If no further buffers are available, will set vktor_parser->buffer and 
 * vktor_parser->last_buffer to NULL.
 * 
 * @param [in,out] parser The parser we are working with
 */
static void 
parser_advance_buffer(vktor_parser *parser)
{
	vktor_buffer *next;
	
	assert(parser->buffer != NULL);
	assert(eobuffer(parser->buffer));
	
	next = parser->buffer->next_buff;
	buffer_free(parser->buffer);
	parser->buffer = next;
	
	if (parser->buffer == NULL) {
		parser->last_buffer = NULL;
	}
}

/**
 * @brief Set the current token just read by the parser
 * 
 * Set the current token just read by the parser. Called when a token is 
 * encountered, before returning from vktor_parse(). The user can then access
 * the token information. Will also take care of freeing any previous token
 * held by the parser.
 * 
 * @param [in,out] parser Parser object
 * @param [in]     token  New token type
 * @param [in]     value  New token value or NULL if no value
 */
static void
parser_set_token(vktor_parser *parser, vktor_token token, void *value)
{
	parser->token_type = token;
	if (parser->token_value != NULL) {
		free(parser->token_value);
	}
	parser->token_value = value;
}

/**
 * @brief add a nesting level to the nesting stack
 * 
 * Add a nesting level to the nesting stack when a new array or object is 
 * encountered. Will make sure that the maximal nesting level is not 
 * overflowed.
 * 
 * @param [in,out] parser    Parser object
 * @param [in]     nest_type nesting type - array or object
 * @param [out]    error     an error struct pointer pointer or NULL
 * 
 * @return Status code - VKTOR_OK or VKTOR_ERROR
 */
static vktor_status
nest_stack_add(vktor_parser *parser, vktor_struct nest_type, 
	vktor_error **error)
{
	assert(parser != NULL);
	
	parser->nest_ptr++;
	if (parser->nest_ptr >= parser->max_nest) {
		set_error(error, VKTOR_ERR_MAX_NEST, 
			"maximal nesting level of %d reached", parser->max_nest);
		return VKTOR_ERROR;
	}
	
	parser->nest_stack[parser->nest_ptr] = nest_type;
	
	return VKTOR_OK;
}

/**
 * @brief pop a nesting level out of the nesting stack
 * 
 * Pop a nesting level out of the nesting stack when the end of an array or an
 * object is encountered. Will ensure there are no stack underflows.
 * 
 * @param [in,out] parser Parser object
 * @param [out]    error struct pointer pointer or NULL
 * 
 * @return Status code: VKTOR_OK or VKTOR_ERROR
 */
static vktor_status
nest_stack_pop(vktor_parser *parser, vktor_error **error)
{
	assert(parser != NULL);
	assert(parser->nest_stack[parser->nest_ptr]);
	
	parser->nest_ptr--;
	if (parser->nest_ptr < 0) {
		set_error(error, VKTOR_ERR_INTERNAL_ERR, 
			"internal parser error: nesting stack pointer underflow");
		return VKTOR_ERROR;
	}
	
	return VKTOR_OK;
}

/**
 * @brief Read a string token
 * 
 * Read a string token until the ending double-quote. Will decode any special
 * escaped characters found along the way, and will gracefully handle buffer 
 * replacement. 
 * 
 * Used by parser_read_string_token() and parser_read_objkey_token()
 * 
 * @param [in,out] parser Parser object
 * @param [out]    error  Error object pointer pointer or NULL
 * 
 * @return Status code
 * 
 * @todo Handle control characters and unicode 
 * @todo Handle quoted special characters 
 */
static vktor_status
parser_read_string(vktor_parser *parser, vktor_error **error)
{
	char           c;
	char          *token;
	int            ptr, maxlen;
	int            done = 0;
	
	assert(parser != NULL);
	
	// Allocate memory for reading the string
	
	if (parser->token_resume) {
		ptr = parser->token_size;
		if (ptr < VKTOR_STR_MEMCHUNK) {
			maxlen = VKTOR_STR_MEMCHUNK;
			token = (void *) parser->token_value;
			assert(token != NULL);
		} else {
			maxlen = ptr + VKTOR_STR_MEMCHUNK;
			token = realloc(parser->token_value, sizeof(char) * maxlen);
		}	
		
	} else {
		token  = malloc(VKTOR_STR_MEMCHUNK * sizeof(char));
		maxlen = VKTOR_STR_MEMCHUNK;
		ptr    = 0;
	}
	
	if (token == NULL) {
		set_error(error, VKTOR_ERR_OUT_OF_MEMORY, 
			"unable to allocate %d bytes for string parsing", 
			VKTOR_STR_MEMCHUNK);
		return VKTOR_ERROR;
	}
	
	// Read string from buffer
	
	while (parser->buffer != NULL) {
		while (! eobuffer(parser->buffer)) {
			c = parser->buffer->text[parser->buffer->ptr];
			
			// Read an escaped character (previous char was '/')
			if (parser->expected == VKTOR_C_ESCAPED) {
				switch (c) {
					case '"':
					case '\\':
					case '/':
						token[ptr++] = c;
						check_reallocate_token_memory(VKTOR_STR_MEMCHUNK);
						parser->expected = VKTOR_T_STRING;
						break;
						
					case 'b':
						token[ptr++] = '\b';
						check_reallocate_token_memory(VKTOR_STR_MEMCHUNK);
						parser->expected = VKTOR_T_STRING;
						break;
						
					case 'f':
						token[ptr++] = '\f';
						check_reallocate_token_memory(VKTOR_STR_MEMCHUNK);
						parser->expected = VKTOR_T_STRING;
						break;
						
					case 'n':
						token[ptr++] = '\n';
						check_reallocate_token_memory(VKTOR_STR_MEMCHUNK);
						parser->expected = VKTOR_T_STRING;
						break;
						
					case 'r':
						token[ptr++] = '\r';
						check_reallocate_token_memory(VKTOR_STR_MEMCHUNK);
						parser->expected = VKTOR_T_STRING;
						break;
					
					case 't':
						token[ptr++] = '\t';
						check_reallocate_token_memory(VKTOR_STR_MEMCHUNK);
						parser->expected = VKTOR_T_STRING;
						break;
						
					case 'u':
						// Read an escaped unicode character
						parser->expected = VKTOR_C_UNIC1;
						break;
						
					default:
						// what is this?
						// throw an error or deal as a regular character?
						set_error_unexpected_c(error, c);
						return VKTOR_ERROR;
						break;
				}
				
			} else if (parser->expected & (VKTOR_C_UNIC1 | 
			                               VKTOR_C_UNIC2 | 
										   VKTOR_C_UNIC3 | 
										   VKTOR_C_UNIC4)) {
				
				// Read an escaped unicode sequence
				c = vktor_unicode_hex_to_int((unsigned char) c);
				switch(parser->expected) {
					
					case VKTOR_C_UNIC1:
						parser->unicode_c = parser->unicode_c | (c << 12);
						parser->expected = VKTOR_C_UNIC2;
						break;
						
					case VKTOR_C_UNIC2:
						parser->unicode_c = parser->unicode_c | (c << 8);
						parser->expected = VKTOR_C_UNIC3;
						break;
						
					case VKTOR_C_UNIC3:
						parser->unicode_c = parser->unicode_c | (c << 4);
						parser->expected = VKTOR_C_UNIC4;
						break;
						
					case VKTOR_C_UNIC4: 
						parser->unicode_c = parser->unicode_c | c;
						parser->expected = parser->expected = VKTOR_T_STRING;
						
						if (VKTOR_UNICODE_HIGH_SURROGATE(parser->unicode_c)) {
							// Expecting a low surrogate
							parser->unicode_c <<= 16;
							parser->expected = VKTOR_C_UNIC_LS;
							
						} else if(parser->unicode_c > 0xffff) {
							// Found the low surrogate pair?
							if (! VKTOR_UNICODE_LOW_SURROGATE((parser->unicode_c & 0x0000ffff))) {
								// invalid low surrogate
								set_error_unexpected_c(error, c);
								return VKTOR_ERROR;
								
							}
							
							// Convert surrogate pair to UTF-8
							unsigned char utf8[5];
							short         l, i;
							
							l = vktor_unicode_sp_to_utf8(
									(unsigned short) (parser->unicode_c >> 16),
									(unsigned short) parser->unicode_c,
									utf8);
							if (l == 0) {
								// invalid surrogate pair
								set_error_unexpected_c(error, c);
								return VKTOR_ERROR;
							}
							                         
							for (i = 0; i < l; i++) {
								token[ptr++] = utf8[i];
							}
							check_reallocate_token_memory(VKTOR_STR_MEMCHUNK);
							parser->unicode_c = 0;
							parser->expected = VKTOR_T_STRING;

						} else {
							unsigned char utf8[4];
							short         l, i;
							
							// Get the character as UTF8 and add it to the string
							l = vktor_unicode_cp_to_utf8((unsigned short) parser->unicode_c, utf8);
							if (l == 0) {
								// invalid Unicode character
								set_error_unexpected_c(error, c);
								return VKTOR_ERROR;
							}
							
							for (i = 0; i < l; i++) {
								token[ptr++] = utf8[i];
							}
							check_reallocate_token_memory(VKTOR_STR_MEMCHUNK);
							parser->unicode_c = 0;
							parser->expected = VKTOR_T_STRING;
						}
						
						break;
						
					default: // should not happen
						set_error(error, VKTOR_ERR_INTERNAL_ERR, 
							"internal parser error: expecing a Unicode sequence character");
						return VKTOR_ERROR;
						break;
				}
			
			} else if (parser->expected == VKTOR_C_UNIC_LS) {
				// Expecting another unicode character
				switch(c) {
					case '\\':
						break;
						
					case 'u':
						parser->expected = VKTOR_C_UNIC1;
						break;
					
					default:
						set_error_unexpected_c(error, c);
						return VKTOR_ERROR;
						break;
				}
				
			} else {
				switch (c) {
					case '"':
						// end of string;
						if (! (parser->expected & VKTOR_T_STRING)) {
							// string should not end yet!
							set_error_unexpected_c(error, c);
							return VKTOR_ERROR;
						}
						done = 1;
						break;
						
					case '\\':
						// Some escaped character
						parser->expected = VKTOR_C_ESCAPED;
						break;
						
					default:
						// Are we expecting a regular char?
						if (! (parser->expected & VKTOR_T_STRING)) {
							set_error_unexpected_c(error, c);
							return VKTOR_ERROR;
						}
						
						// Unicode control characters must be escaped
						if (c >= 0 && c <= 0x1f) {
							set_error_unexpected_c(error, c);
							return VKTOR_ERROR;
						}
						
						token[ptr++] = c;
						check_reallocate_token_memory(VKTOR_STR_MEMCHUNK);
						break;
				}
			}
						
			parser->buffer->ptr++;
			if (done) break;
		}
		
		if (done) break;
		parser_advance_buffer(parser);
	}
	
	parser->token_value = (void *) token;
	parser->token_size  = ptr;
	
	// Check if we need more data
	if (! done) {
		parser->token_resume = 1;
		return VKTOR_MORE_DATA;
	} else {
		token[ptr] = '\0';
		parser->token_resume = 0;
		return VKTOR_OK;
	}
}

/**
 * @brief Read a string token
 * 
 * Read a string token using parser_read_string() and set the next expected 
 * token map accordingly
 * 
 * @param [in,out] parser Parser object
 * @param [out]    error  Error object pointer pointer or null
 * 
 * @return Status code
 */
static vktor_status
parser_read_string_token(vktor_parser *parser, vktor_error **error)
{
	vktor_status status;
	
	if (! parser->token_resume) {
		parser_set_token(parser, VKTOR_T_STRING, NULL);
	}
	
	// Read string	
	status = parser_read_string(parser, error);
	
	// Set next expected token
	if (status == VKTOR_OK) {
		expect_next_value_token(parser);
	}
	
	return status;
}

/**
 * @brief Read an object key token
 * 
 * Read an object key using parser_read_string() and set the next expected 
 * token map accordingly
 * 
 * @param [in,out] parser Parser object
 * @param [out]    error  Error object pointer pointer or null
 * 
 * @return Status code
 */
static vktor_status
parser_read_objkey_token(vktor_parser *parser, vktor_error **error)
{
	vktor_status status;
	
	assert(nest_stack_in(parser, VKTOR_STRUCT_OBJECT));
	
	// Expecting a string
	parser->expected = VKTOR_T_STRING;
	
	if (! parser->token_resume) {
		parser_set_token(parser, VKTOR_T_OBJECT_KEY, NULL);
	}
	
	// Read string	
	status = parser_read_string(parser, error);
	
	// Set next expected token
	if (status == VKTOR_OK) {
		parser->expected = VKTOR_C_COLON;
	}
	
	return status;
}

/**
 * @brief Read an "expected" token
 * 
 * Read an "expected" token - used when we guess in advance what the next token
 * should be and want to try reading it. 
 * 
 * Used internally to read true, false, and null tokens.
 * 
 * If during the parsing the input does not match the expected token, an error
 * is returned.
 * 
 * @param [in,out] parser Parser object
 * @param [in]     expect Expected token as string
 * @param [in]     explen Expected token length
 * @param [out]    error  Error object pointer pointer or NULL
 * 
 * @return Status code
 */
static vktor_status
parser_read_expectedstr(vktor_parser *parser, const char *expect, int explen, 
	vktor_error **error)
{
	char  c;
	//~ int   ptr;
	
	assert(parser != NULL);
	assert(expect != NULL);
	
	// Expected string should be "null", "true" or "false"
	assert(explen > 3 && explen < 6);
	
	if (! parser->token_resume) {
		parser->token_size = 0;
	}
	
	for (; parser->token_size < explen; parser->token_size++) {
		if (parser->buffer == NULL) {
			parser->token_resume = 1;
			return VKTOR_MORE_DATA;
		}
		
		if (eobuffer(parser->buffer)) {
			parser_advance_buffer(parser);
			if (parser->buffer == NULL) {
				parser->token_resume = 1;
				return VKTOR_MORE_DATA;
			}
		}
		
		c = parser->buffer->text[parser->buffer->ptr];
		if (expect[parser->token_size] != c) {
			set_error_unexpected_c(error, c);
			return VKTOR_ERROR;
		}
		
		parser->buffer->ptr++;
	}
	
	// if we made it here, it means we are good!
	parser->token_size = 0;
	parser->token_resume = 0;
	return VKTOR_OK;
}

/**
 * @brief Read an expected null token
 * 
 * Read an expected null token using parser_read_expectedstr(). Will also set 
 * the next expected token map.
 * 
 * @param [in,out] parser Parser object
 * @param [out]    error  Error object pointer pointer or NULL
 * 
 * @return Status code
 */
static vktor_status
parser_read_null(vktor_parser *parser, vktor_error **error)
{
	vktor_status st = parser_read_expectedstr(parser, "null", 4, error);
	
	if (st != VKTOR_ERROR) {
		parser_set_token(parser, VKTOR_T_NULL, NULL);
		if (st == VKTOR_OK) {
			// Set the next expected token
			expect_next_value_token(parser);
		}
	}
	
	return st;
}

/**
 * @brief Read an expected true token
 * 
 * Read an expected true token using parser_read_expectedstr(). Will also set 
 * the next expected token map.
 * 
 * @param [in,out] parser Parser object
 * @param [out]    error  Error object pointer pointer or NULL
 * 
 * @return Status code
 */
static vktor_status
parser_read_true(vktor_parser *parser, vktor_error **error)
{
	vktor_status st = parser_read_expectedstr(parser, "true", 4, error);
	
	if (st != VKTOR_ERROR) {
		parser_set_token(parser, VKTOR_T_TRUE, NULL);
		if (st == VKTOR_OK) {
			// Set the next expected token
			expect_next_value_token(parser);
		}
	}
	
	return st;
}

/**
 * @brief Read an expected false token
 * 
 * Read an expected false token using parser_read_expectedstr(). Will also set 
 * the next expected token map.
 * 
 * @param [in,out] parser Parser object
 * @param [out]    error  Error object pointer pointer or NULL
 * 
 * @return Status code
 */
static vktor_status
parser_read_false(vktor_parser *parser, vktor_error **error)
{
	vktor_status st = parser_read_expectedstr(parser, "false", 5, error);
	
	if (st != VKTOR_ERROR) {
		parser_set_token(parser, VKTOR_T_FALSE, NULL);
		if (st == VKTOR_OK) {
			// Set the next expected token
			expect_next_value_token(parser);
		}
	}
	
	return st;
}

/**
 * @brief Read a number token
 * 
 * Read a number token - this might be an integer or a floating point number.
 * Will set the token_type accordingly. 
 * 
 * @param [in,out] parser Parser object
 * @param [out]    error  Error object pointer pointer
 * 
 * @return Status code
 */
static vktor_status 
parser_read_number_token(vktor_parser *parser, vktor_error **error)
{
	char  c;
	char *token;
	int   ptr, maxlen;
	int   done = 0;
	
	assert(parser != NULL);
	
	if (parser->token_resume) {
		ptr = parser->token_size;
		if (ptr < VKTOR_NUM_MEMCHUNK) {
			maxlen = VKTOR_NUM_MEMCHUNK;
			token = (void *) parser->token_value;
			assert(token != NULL);
		} else {
			maxlen = ptr + VKTOR_NUM_MEMCHUNK;
			token = realloc(parser->token_value, sizeof(char) * maxlen);
		}
		
	} else {
		token  = malloc(VKTOR_NUM_MEMCHUNK * sizeof(char));
		maxlen = VKTOR_NUM_MEMCHUNK;
		ptr    = 0;
		
		// Reading a new token - set possible expected characters
		parser->expected = VKTOR_T_INT    | 
		                   VKTOR_T_FLOAT  | 
						   VKTOR_C_DOT    | 
						   VKTOR_C_EXP    | 
						   VKTOR_C_SIGNUM;
						   
		// Free previous token and set token type to INT until proven otherwise 
		parser_set_token(parser, VKTOR_T_INT, NULL);
	}
	
	if (token == NULL) {
		set_error(error, VKTOR_ERR_OUT_OF_MEMORY, 
			"unable to allocate %d bytes for string parsing", 
			VKTOR_NUM_MEMCHUNK);
		return VKTOR_ERROR;
	}
	
	while (parser->buffer != NULL) {
		while (! eobuffer(parser->buffer)) {
			c = parser->buffer->text[parser->buffer->ptr];
			
			switch (c) {
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					// Digits are always allowed
					token[ptr++] = c;
					
					// Signum cannot come after a digit
					parser->expected = (parser->expected & ~VKTOR_C_SIGNUM);
					break;
					
				case '.':
					if (! (parser->expected & VKTOR_C_DOT && ptr > 0)) {
						set_error_unexpected_c(error, c);
						return VKTOR_ERROR;
					}
					
					token[ptr++] = c;
					
					// Dots are no longer allowed
					parser->expected = parser->expected & ~VKTOR_C_DOT;
					
					// This is a floating point number
					parser->token_type = VKTOR_T_FLOAT;
					break;
					
				case '-':
				case '+':
					if (! (parser->expected & VKTOR_C_SIGNUM)) {
						set_error_unexpected_c(error, c);
						return VKTOR_ERROR;
					}
					
					token[ptr++] = c;
				
					// Signum is no longer allowed
					parser->expected = parser->expected & ~VKTOR_C_SIGNUM;
					break;
					
				case 'e':
				case 'E':
					if (! (parser->expected & VKTOR_C_EXP && ptr > 0)) {
						set_error_unexpected_c(error, c);
						return VKTOR_ERROR;
					}
					
					// Make sure the previous sign is a number
					switch(token[ptr - 1]) {
						case '.':
						case '+':
						case '-':
							set_error_unexpected_c(error, c);
							return VKTOR_ERROR;
							break;
					}
					
					// Exponent is no longer allowed 
					parser->expected = parser->expected & ~VKTOR_C_EXP;
					
					// Dot is no longer allowed
					parser->expected = parser->expected & ~VKTOR_C_DOT;
					
					// Signum is now allowed again
					parser->expected = parser->expected | VKTOR_C_SIGNUM;
					
					// This is a floating point number
					parser->token_type = VKTOR_T_FLOAT;
					
					token[ptr++] = 'e';
					break;
					
				default:
					// Check that we are not expecting more digits
					assert(ptr > 0);
					switch(token[ptr - 1]) {
						case 'e':
						case 'E':
						case '.':
						case '+':
						case '-':
							set_error_unexpected_c(error, c);
							return VKTOR_ERROR;
							break;
					}
					
					done = 1;
					break;	
			}
			
			if (done) break;
			parser->buffer->ptr++;
			check_reallocate_token_memory(VKTOR_NUM_MEMCHUNK);
		}
		
		if (done) break;
		parser_advance_buffer(parser);
	}
	
	parser->token_value = (void *) token;
	parser->token_size  = ptr;
	
	// Check if we need more data
	if (! done) {
		parser->token_resume = 1;
		return VKTOR_MORE_DATA;
	} else {
		token[ptr] = '\0';
		parser->token_resume = 0;
		expect_next_value_token(parser);
		return VKTOR_OK;
	}
}

/** @} */ // end of internal PAI

/**
 * External API
 * 
 * @defgroup external External API
 * @{
 */

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
vktor_parser*
vktor_parser_init(int max_nest)
{
	vktor_parser *parser;
	
	if ((parser = malloc(sizeof(vktor_parser))) == NULL) {
		return NULL;
	}
		
	parser->buffer       = NULL;
	parser->last_buffer  = NULL;
	parser->token_type   = VKTOR_T_NONE;
	parser->token_value  = NULL;
	parser->token_resume = 0;
	
	// set expectated tokens
	parser->expected   = VKTOR_VALUE_TOKEN;

	// set up nesting stack
	parser->nest_stack   = calloc(sizeof(vktor_struct), max_nest);
	parser->nest_ptr     = 0;
	parser->max_nest     = max_nest;
			
	return parser;
}

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
vktor_status 
vktor_feed(vktor_parser *parser, char *text, long text_len, 
           vktor_error **err) 
{
	vktor_buffer *buffer;
	
	// Create buffer
	if ((buffer = buffer_init(text, text_len)) == NULL) {
		set_error(err, VKTOR_ERR_OUT_OF_MEMORY, 
			"Unable to allocate memory buffer for %ld bytes", text_len);
		return VKTOR_ERROR;
	}
	
	// Link buffer to end of parser buffer chain
	if (parser->last_buffer == NULL) {
		assert(parser->buffer == NULL);
		parser->buffer = buffer;
		parser->last_buffer = buffer;
	} else {
		parser->last_buffer->next_buff = buffer;
		parser->last_buffer = buffer;
	}
	
	return VKTOR_OK;
}

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
 * @return status code:
 *  - VKTOR_OK        if a token was encountered
 *  - VKTOR_ERROR     if an error has occured
 *  - VKTOR_MORE_DATA if we need more data in order to continue parsing
 *  - VKTOR_COMPLETE  if parsing is complete and no further data is expected
 */
vktor_status 
vktor_parse(vktor_parser *parser, vktor_error **error)
{
	char c;
	int  done;
	
	assert(parser != NULL);
	
	// Do we have a buffer to work with?
	while (parser->buffer != NULL) {
		done = 0;
		
		// Do we need to continue reading the previous token?
		if (parser->token_resume) {
						
		    switch (parser->token_type) {
		    	case VKTOR_T_OBJECT_KEY:
		    		return parser_read_objkey_token(parser, error);
		    		break;
		    		
		    	case VKTOR_T_STRING:
		    		return parser_read_string_token(parser, error);
		    		break;
		    	
				case VKTOR_T_NULL:
					return parser_read_null(parser, error);
					break;
					
				case VKTOR_T_TRUE:
					return parser_read_true(parser, error);
					break;
				
				case VKTOR_T_FALSE:
					return parser_read_false(parser, error);
					break;
				
				case VKTOR_T_INT:
				case VKTOR_T_FLOAT:
					return parser_read_number_token(parser, error);
					break;
					
		    	default:
		    		set_error(error, VKTOR_ERR_INTERNAL_ERR, 
		    			"token resume flag is set but token type %d is unexpected",
		    			parser->token_type);
		    		return VKTOR_ERROR;
		    		break;
		    }
		}
		
		while (! eobuffer(parser->buffer)) {
			c = parser->buffer->text[parser->buffer->ptr];
			
			switch (c) {
				case '{':
					if (! parser->expected & VKTOR_T_OBJECT_START) {
						set_error_unexpected_c(error, c);
						return VKTOR_ERROR;
					}
					
					if (nest_stack_add(parser, VKTOR_STRUCT_OBJECT, error) == VKTOR_ERROR) {
						return VKTOR_ERROR;
					}
					
					parser_set_token(parser, VKTOR_T_OBJECT_START, NULL);
					
					// Expecting: object key or object end
					parser->expected = VKTOR_T_OBJECT_KEY |
					                   VKTOR_T_OBJECT_END;
					
					done = 1;
					break;
					
				case '[':
					if (! parser->expected & VKTOR_T_ARRAY_START) {
						set_error_unexpected_c(error, c);
						return VKTOR_ERROR;
					}
					
					if (nest_stack_add(parser, VKTOR_STRUCT_ARRAY, error) == VKTOR_ERROR) {
						return VKTOR_ERROR;
					}
					
					parser_set_token(parser, VKTOR_T_ARRAY_START, NULL);
					
					// Expecting: any value or array end
					parser->expected = VKTOR_VALUE_TOKEN | 
					                     VKTOR_T_ARRAY_END;
					
					done = 1;
					break;
					
				case '"':
					if (! parser->expected & (VKTOR_T_STRING | 
					                          VKTOR_T_OBJECT_KEY)) {
						set_error_unexpected_c(error, c);
						return VKTOR_ERROR;
					}
					
					parser->buffer->ptr++;	 
					
					if (parser->expected & VKTOR_T_OBJECT_KEY) {
						return parser_read_objkey_token(parser, error);
					} else {
						return parser_read_string_token(parser, error);
					}
					
					break;
				
				case ',':
					if (! parser->expected & VKTOR_C_COMMA) {
						set_error_unexpected_c(error, c);
						return VKTOR_ERROR;
					}
					
					switch(parser->nest_stack[parser->nest_ptr]) {
						case VKTOR_STRUCT_OBJECT:
							parser->expected = VKTOR_T_OBJECT_KEY;
							break;
							
						case VKTOR_STRUCT_ARRAY:
							parser->expected = VKTOR_VALUE_TOKEN;
							break;
							
						default:
							set_error(error, VKTOR_ERR_INTERNAL_ERR, 
								"internal parser error: unexpected nesting stack member");
							return VKTOR_ERROR;
							break;
					}
					break;
				
				case ':':
					if (! parser->expected & VKTOR_C_COLON) {
						set_error_unexpected_c(error, c);
						return VKTOR_ERROR;
					}
					
					// Colon is only expected inside objects
					assert(nest_stack_in(parser, VKTOR_STRUCT_OBJECT));
					
					// Next we expected a value
					parser->expected = VKTOR_VALUE_TOKEN;
					break;
					
				case '}':
					if (! (parser->expected & VKTOR_T_OBJECT_END &&
					       nest_stack_in(parser, VKTOR_STRUCT_OBJECT))) {
					
						set_error_unexpected_c(error, c);
						return VKTOR_ERROR;
					}
					
					parser_set_token(parser, VKTOR_T_OBJECT_END, NULL);
					
					if (nest_stack_pop(parser, error) == VKTOR_ERROR) {
						return VKTOR_ERROR;
					} 
					
					if (parser->nest_ptr > 0) {
						// Next can be either a comma, or end of array / object
						parser->expected = VKTOR_C_COMMA | 
											 VKTOR_T_OBJECT_END | 
											 VKTOR_T_ARRAY_END;
					} else {
						// Next can be nothing
						parser->expected = VKTOR_T_NONE;
					}
					                     
					done = 1;
					break;
					
				case ']':
					if (! (parser->expected & VKTOR_T_ARRAY_END &&
					       nest_stack_in(parser, VKTOR_STRUCT_ARRAY))) { 
					
						set_error_unexpected_c(error, c);
						return VKTOR_ERROR;
					}
					parser_set_token(parser, VKTOR_T_ARRAY_END, NULL);
					
					if (nest_stack_pop(parser, error) == VKTOR_ERROR) {
						return VKTOR_ERROR;
					} 
					
					if (parser->nest_ptr > 0) {
						// Next can be either a comma, or end of array / object
						parser->expected = VKTOR_C_COMMA      | 
						                   VKTOR_T_OBJECT_END | 
										   VKTOR_T_ARRAY_END;
					} else {
						// Next can be nothing
						parser->expected = VKTOR_T_NONE;
					}
					                     
					done = 1;
					break;
					
				case ' ':
				case '\n':
				case '\r':
				case '\t':
				case '\f':
				case '\v':
					// Whitespace - do nothing!
					/** 
					 * @todo consinder: read all whitespace without looping? 
					 */
					break;
					
				case 't':
					// true?
					if (! parser->expected & VKTOR_T_TRUE) {
						set_error_unexpected_c(error, c);
						return VKTOR_ERROR;
					}
					
					return parser_read_true(parser, error);
					break;
					
				case 'f':
					// false?
					if (! parser->expected & VKTOR_T_FALSE) {
						set_error_unexpected_c(error, c);
						return VKTOR_ERROR;
					}
					
					return parser_read_false(parser, error);
					break;

				case 'n':
					// null?
					if (! parser->expected & VKTOR_T_NULL) {
						set_error_unexpected_c(error, c);
						return VKTOR_ERROR;
					}
					
					return parser_read_null(parser, error);
					break;
									
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				case '-':
				case '+':
					// Read a number
					if (! parser->expected & (VKTOR_T_INT | 
					                          VKTOR_T_FLOAT)) {
						set_error_unexpected_c(error, c);
						return VKTOR_ERROR;
					}
					
					return parser_read_number_token(parser, error);
					break;
					
				default:
					// Unexpected character
					set_error_unexpected_c(error, c);
					return VKTOR_ERROR;
					break;
			}
			
			parser->buffer->ptr++;
			if (done) break;
		}
		
		if (done) break;
		parser_advance_buffer(parser);	
	}
	
	assert(parser->nest_ptr >= 0);
	
	if (parser->buffer != NULL) {
		return VKTOR_OK;
	} else {
		if (parser->nest_ptr == 0 && parser->token_type != VKTOR_T_NONE) {
			return VKTOR_COMPLETE;
		} else {
			return VKTOR_MORE_DATA;
		}
	}
}

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
int
vktor_get_depth(vktor_parser *parser) 
{
	return parser->nest_ptr;	
}

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
vktor_struct
vktor_get_current_struct(vktor_parser *parser)
{
	assert(parser != NULL);
	return parser->nest_stack[parser->nest_ptr];
}

/**
 * @brief Get the current token type
 * 
 * Get the type of the current token pointed to by the parser
 * 
 * @param [in] parser Parser object
 * 
 * @return Token type (one of the VKTOR_T_* tokens)
 */
vktor_token
vktor_get_token_type(vktor_parser *parser)
{
	assert(parser != NULL);
	return parser->token_type;	
}

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
long 
vktor_get_value_long(vktor_parser *parser, vktor_error **error)
{
	long val;
	
	assert(parser != NULL);
	
	if (parser->token_value == NULL) {
		set_error(error, VKTOR_ERR_NO_VALUE, "token value is unknown");
		return 0;
	}
	
	errno = 0;
	val = strtol((char *) parser->token_value, NULL, 10);
	if (errno == ERANGE) {
		set_error(error, VKTOR_ERR_OUT_OF_RANGE,
			"integer value overflows maximal long value");
		return 0;
	}
	
	return val;
}

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
double 
vktor_get_value_double(vktor_parser *parser, vktor_error **error)
{
	double val;
	
	assert(parser != NULL);
	
	if (parser->token_value == NULL) {
		set_error(error, VKTOR_ERR_NO_VALUE, "token value is unknown");
		return 0;
	}
	
	errno = 0;
	val = strtod((char *) parser->token_value, NULL);
	if (errno == ERANGE) {
		set_error(error, VKTOR_ERR_OUT_OF_RANGE,
			"number value overflows maximal double value");
		return 0;
	}
	
	return val;
}

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
int
vktor_get_value_str(vktor_parser *parser, char **val, vktor_error **error)
{
	assert(parser != NULL);
	
	if (parser->token_value == NULL) {
		set_error(error, VKTOR_ERR_NO_VALUE, "token value is unknown");
		return -1;
	}
	
	*val = (char *) parser->token_value;
	return parser->token_size;
}

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
int 
vktor_get_value_str_copy(vktor_parser *parser, char **val, vktor_error **error)
{
	char *str;
	
	assert(parser != NULL);
	
	if (parser->token_value == NULL) {
		set_error(error, VKTOR_ERR_NO_VALUE, "token value is unknown");
		return 0;
	}
	
	str = malloc(sizeof(char) * (parser->token_size + 1));
	str = memcpy(str, parser->token_value, parser->token_size);
	str[parser->token_size] = '\0';
	
	*val = str;
	return parser->token_size;
}

/**
 * @brief Free a parser and any associated memory
 * 
 * Free a parser and any associated memory structures, including any linked
 * buffers
 * 
 * @param [in,out] parser parser struct to free
 */
void
vktor_parser_free(vktor_parser *parser)
{
	assert(parser != NULL);
	
	if (parser->buffer != NULL) {
		buffer_free_all(parser->buffer);
	}
	
	if (parser->token_value != NULL) {
		free(parser->token_value);
	}
	
	free(parser->nest_stack);
	
	free(parser);
}

/**
 * @brief Free an error struct
 * 
 * Free an error struct
 * 
 * @param [in,out] err Error struct to free
 */
void 
vktor_error_free(vktor_error *err)
{
	if (err->message != NULL) {
		free(err->message);
	}
	
	free(err);
}

/** @} */ // end of external API
