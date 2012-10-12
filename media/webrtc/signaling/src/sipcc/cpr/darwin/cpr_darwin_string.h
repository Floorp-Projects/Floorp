/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_DARWIN_STRING_H_
#define _CPR_DARWIN_STRING_H_

#include <string.h>
#include <ctype.h>

/**
 * cpr_strdup
 *
 * @brief The CPR wrapper for strdup

 * The cpr_strdup shall return a pointer to a new string, which is a duplicate
 * of the string pointed to by "str" argument. A null pointer is returned if the
 * new string cannot be created.
 *
 * @param[in] str  - The string that needs to be duplicated
 *
 * @return The duplicated string or NULL in case of no memory
 *
 */
char *
cpr_strdup(const char *str);

/**
 * strcasestr
 *
 * @brief The same as strstr, but ignores case
 *
 * The strcasestr performs the strstr function, but ignores the case.
 * This function shall locate the first occurrence in the string
 * pointed to by s1 of the sequence of bytes (excluding the terminating
 * null byte) in the string pointed to by s2.
 *
 * @param[in] s1  - The input string
 * @param[in] s2  - The pattern to be matched
 *
 * @return A pointer to the first occurrence of string s2 found
 *           in string s1 or NULL if not found.  If s2 is an empty
 *           string then s1 is returned.
 */
char *
strcasestr(const char *s1, const char *s2);


#endif
