/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _CPR_STRING_H_
#define _CPR_STRING_H_

#include "cpr_types.h"
#include "cpr_strings.h"

__BEGIN_DECLS

#if defined SIP_OS_LINUX
#include "../linux/cpr_linux_string.h"
#elif defined SIP_OS_WINDOWS
#include "../win32/cpr_win_string.h"
#define cpr_strdup _strdup
#elif defined SIP_OS_OSX
#include "../darwin/cpr_darwin_string.h"
#endif

/**
 * sstrncpy
 *
 * @brief The CPR wrapper for strncpy
 *
 * This is Cisco's *safe* version of strncpy.  The string will always
 * be NUL terminated (which is not ANSI compliant).
 *
 * @param[in] dst  - The destination string
 * @param[in] src  - The source
 * @param[in]  max - maximum length in octets to concatenate
 *
 * @return     Pointer to the @b end of the string
 *
 * @note       Modified to be explicitly safe for all inputs.
 *             Also return the number of characters copied excluding the
 *             NUL terminator vs. the original string s1.  This simplifies
 *             code where sstrncat functions follow.
 */
unsigned long
sstrncpy(char *dst, const char *src, unsigned long max);


/**
 * sstrncat
 * 
 * @brief The CPR wrapper for strncat
 *
 * This is Cisco's *safe* version of strncat.  The string will always
 * be NUL terminated (which is not ANSI compliant).
 *
 * @param[in] s1  - first string
 * @param[in] s2  - second string
 * @param[in]  max - maximum length in octets to concatenate
 *
 * @return     Pointer to the @b end of the string
 *
 * @note    Modified to be explicitly safe for all inputs.
 *             Also return the end vs. the beginning of the string s1
 *             which is useful for multiple sstrncat calls.
 */
char *
sstrncat(char *s1, const char *s2, unsigned long max);

/*
 * flex_string
 */
#define FLEX_STRING_CHUNK_SIZE 256

typedef struct {
  char *buffer;
  size_t buffer_length;
  size_t string_length;
} flex_string;

/*
 * flex_string_init
 *
 * Not thread-safe
 */
void flex_string_init(flex_string *fs);

/*
 * flex_string_free
 *
 * Not thread-safe
 */
void flex_string_free(flex_string *fs);

/*
 * flex_string_check_alloc
 *
 * Allocate enough chunks to hold the new minimum size.
 *
 * Not thread-safe
 */
void flex_string_check_alloc(flex_string *fs, size_t new_min_length);

/*
 * flex_string_append
 *
 * Not thread-safe
 */
void flex_string_append(flex_string *fs, const char *more);

/*
 * flex_string_sprintf
 *
 * Not thread-safe
 */
void flex_string_sprintf(flex_string *fs, const char *format, ...);

__END_DECLS

#endif
