/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_STRING_H_
#define _CPR_STRING_H_

#include <stdarg.h>

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
void flex_string_vsprintf(flex_string *fs, const char *format, va_list original_ap);

/*
 * flex_string_sprintf
 *
 * Not thread-safe
 */
void flex_string_sprintf(flex_string *fs, const char *format, ...);

__END_DECLS

#endif
