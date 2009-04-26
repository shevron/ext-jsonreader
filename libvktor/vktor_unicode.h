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
 * @file vktor_unicode.h
 * 
 * vktor Unicode header file - Unicode related functions
 * 
 * @internal
 */

#ifndef _VKTOR_UNICODE_H

/**
 * @ingroup internal
 * @{
 */

/**
 * Convenience macro to check if a codepoint is a high surrogate
 */
#define VKTOR_UNICODE_HIGH_SURROGATE(cp) (cp >= 0xd800 && cp <= 0xdbff)

/**
 * Convenience macro to check if a codepoint is a low surrogate
 */
#define VKTOR_UNICODE_LOW_SURROGATE(cp) (cp >= 0xdc99 && cp <= 0xdfff)

/**
 * @brief Convert a hexadecimal digit to it's integer value
 * 
 * Convert a single char containing a hexadecimal digit to it's integer value
 * (0 - 15). Used when converting escaped Unicode sequences to UTF-* characters.
 * 
 * Note that this function does not do any error checking because it is for 
 * internal use only. If the character is not a valid hex digit, 0 is returned. 
 * Obviously since 0 is a valid value it should not be used for error checking.
 * 
 * @param [in] hex Hexadecimal character
 * 
 * @return Integer value (0 - 15). 
 */
unsigned char vktor_unicode_hex_to_int(unsigned char hex);

/**
 * @brief Encode a Unicode code point to a UTF-8 string
 * 
 * Encode a 16 bit number representing a Unicode code point (UCS-2) to 
 * a UTF-8 encoded string. Note that this function does not handle surrogate
 * pairs - you should use vktor_uncode_sp_to_utf8() in this case.
 * 
 * @param [in]  cp    the unicode codepoint
 * @param [out] utf8  a pointer to a 5 byte long string (at least) that will 
 *   be populated with the UTF-8 encoded string
 * 
 * @return the length of the UTF-8 string (1 - 3 bytes) or 0 in case of error
 */
short vktor_unicode_cp_to_utf8(unsigned short cp, unsigned char *utf8);

/**
 * @brief Convert a UTF-16 surrogate pair to a UTF-8 character
 *
 * Converts a UTF-16 surrogate pair (two 16 bit characters) into a single 4-byte
 * UTF-8 character. This function should be called only after checking that
 * you have a valid surrogate pair.
 *
 * @param [in]  high High surrogate
 * @param [in]  low  Low  surrogate
 * @param [out] utf8 Resulting UTF-8 character
 *
 * @return Byte-length of resulting character - should be 4, or 0 if there's an
 *   error.
 */
short vktor_unicode_sp_to_utf8(unsigned short high, unsigned short low, unsigned char *utf8);

/** @} */ // end of internal API

#define _VKTOR_UNICODE_H
#endif /* VKTOR_UNICODE_H */
