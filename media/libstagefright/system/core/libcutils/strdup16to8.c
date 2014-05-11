/* libs/cutils/strdup16to8.c
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#include <limits.h>  /* for SIZE_MAX */

#include <cutils/jstring.h>
#include <assert.h>
#include <stdlib.h>


/**
 * Given a UTF-16 string, compute the length of the corresponding UTF-8
 * string in bytes.
 */
extern size_t strnlen16to8(const char16_t* utf16Str, size_t len)
{
    size_t utf8Len = 0;

    /* A small note on integer overflow. The result can
     * potentially be as big as 3*len, which will overflow
     * for len > SIZE_MAX/3.
     *
     * Moreover, the result of a strnlen16to8 is typically used
     * to allocate a destination buffer to strncpy16to8 which
     * requires one more byte to terminate the UTF-8 copy, and
     * this is generally done by careless users by incrementing
     * the result without checking for integer overflows, e.g.:
     *
     *   dst = malloc(strnlen16to8(utf16,len)+1)
     *
     * Due to this, the following code will try to detect
     * overflows, and never return more than (SIZE_MAX-1)
     * when it detects one. A careless user will try to malloc
     * SIZE_MAX bytes, which will return NULL which can at least
     * be detected appropriately.
     *
     * As far as I know, this function is only used by strndup16(),
     * but better be safe than sorry.
     */

    /* Fast path for the usual case where 3*len is < SIZE_MAX-1.
     */
    if (len < (SIZE_MAX-1)/3) {
        while (len--) {
            unsigned int uic = *utf16Str++;

            if (uic > 0x07ff)
                utf8Len += 3;
            else if (uic > 0x7f || uic == 0)
                utf8Len += 2;
            else
                utf8Len++;
        }
        return utf8Len;
    }

    /* The slower but paranoid version */
    while (len--) {
        unsigned int  uic     = *utf16Str++;
        size_t        utf8Cur = utf8Len;

        if (uic > 0x07ff)
            utf8Len += 3;
        else if (uic > 0x7f || uic == 0)
            utf8Len += 2;
        else
            utf8Len++;

        if (utf8Len < utf8Cur) /* overflow detected */
            return SIZE_MAX-1;
    }

    /* don't return SIZE_MAX to avoid common user bug */
    if (utf8Len == SIZE_MAX)
        utf8Len = SIZE_MAX-1;

    return utf8Len;
}


/**
 * Convert a Java-Style UTF-16 string + length to a JNI-Style UTF-8 string.
 *
 * This basically means: embedded \0's in the UTF-16 string are encoded
 * as "0xc0 0x80"
 *
 * Make sure you allocate "utf8Str" with the result of strlen16to8() + 1,
 * not just "len".
 *
 * Please note, a terminated \0 is always added, so your result will always
 * be "strlen16to8() + 1" bytes long.
 */
extern char* strncpy16to8(char* utf8Str, const char16_t* utf16Str, size_t len)
{
    char* utf8cur = utf8Str;

    /* Note on overflows: We assume the user did check the result of
     * strnlen16to8() properly or at a minimum checked the result of
     * its malloc(SIZE_MAX) in case of overflow.
     */
    while (len--) {
        unsigned int uic = *utf16Str++;

        if (uic > 0x07ff) {
            *utf8cur++ = (uic >> 12) | 0xe0;
            *utf8cur++ = ((uic >> 6) & 0x3f) | 0x80;
            *utf8cur++ = (uic & 0x3f) | 0x80;
        } else if (uic > 0x7f || uic == 0) {
            *utf8cur++ = (uic >> 6) | 0xc0;
            *utf8cur++ = (uic & 0x3f) | 0x80;
        } else {
            *utf8cur++ = uic;

            if (uic == 0) {
                break;
            }
        }
    }

   *utf8cur = '\0';

   return utf8Str;
}

/**
 * Convert a UTF-16 string to UTF-8.
 *
 */
char * strndup16to8 (const char16_t* s, size_t n)
{
    char*   ret;
    size_t  len;

    if (s == NULL) {
        return NULL;
    }

    len = strnlen16to8(s, n);

    /* We are paranoid, and we check for SIZE_MAX-1
     * too since it is an overflow value for our
     * strnlen16to8 implementation.
     */
    if (len >= SIZE_MAX-1)
        return NULL;

    ret = malloc(len + 1);
    if (ret == NULL)
        return NULL;

    strncpy16to8 (ret, s, n);

    return ret;
}
