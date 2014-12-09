/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdarg.h>

#include "mozilla/Assertions.h"
#include "cpr_types.h"
#include "cpr_stdlib.h"
#include "cpr_string.h"
#include "cpr_strings.h"

/**
 * sstrncpy
 *
 * This is Cisco's *safe* version of strncpy.  The string will always
 * be NUL terminated (which is not ANSI compliant).
 *
 * Parameters: s1  - first string
 *             s2  - second string
 *             max - maximum length in octets to concat.
 *
 * Return:     Pointer to the *end* of the string
 *
 * Remarks:    Modified to be explicitly safe for all inputs.
 *             Also return the number of characters copied excluding the
 *             NUL terminator vs. the original string s1.  This simplifies
 *             code where sstrncat functions follow.
 */
unsigned long
sstrncpy (char *dst, const char *src, unsigned long max)
{
    unsigned long cnt = 0;

    if (dst == NULL) {
        return 0;
    }

    if (src) {
        while ((max-- > 1) && (*src)) {
            *dst = *src;
            dst++;
            src++;
            cnt++;
        }
    }

#if defined(CPR_SSTRNCPY_PAD)
    /*
     * To be equivalent to the TI compiler version
     * v2.01, SSTRNCPY_PAD needs to be defined
     */
    while (max-- > 1) {
        *dst = '\0';
        dst++;
    }
#endif
    *dst = '\0';

    return cnt;
}

/**
 * sstrncat
 *
 * This is Cisco's *safe* version of strncat.  The string will always
 * be NUL terminated (which is not ANSI compliant).
 *
 * Parameters: s1  - first string
 *             s2  - second string
 *             max - maximum length in octets to concatenate
 *
 * Return:     Pointer to the *end* of the string
 *
 * Remarks:    Modified to be explicitly safe for all inputs.
 *             Also return the end vs. the beginning of the string s1
 *             which is useful for multiple sstrncat calls.
 */
char *
sstrncat (char *s1, const char *s2, unsigned long max)
{
    if (s1 == NULL)
        return (char *) NULL;

    while (*s1)
        s1++;

    if (s2) {
        while ((max-- > 1) && (*s2)) {
            *s1 = *s2;
            s1++;
            s2++;
        }
    }
    *s1 = '\0';

    return s1;
}

/*
 * flex_string
 */

/*
 * flex_string_init
 *
 * Not thread-safe
 */
void flex_string_init(flex_string *fs) {
  fs->buffer_length = FLEX_STRING_CHUNK_SIZE;
  fs->string_length = 0;
  fs->buffer = cpr_malloc(fs->buffer_length);
  fs->buffer[0] = '\0';
}

/*
 * flex_string_free
 *
 * Not thread-safe
 */
void flex_string_free(flex_string *fs) {
  fs->buffer_length = 0;
  fs->string_length = 0;
  cpr_free(fs->buffer);
  fs->buffer = NULL;
}

/* For sanity check before alloc */
#define FLEX_STRING_MAX_SIZE (10 * 1024 * 1024) /* 10MB */

/*
 * flex_string_check_alloc
 *
 * Allocate enough chunks to hold the new minimum size.
 *
 * Not thread-safe
 */
void flex_string_check_alloc(flex_string *fs, size_t new_min_length) {
  if (new_min_length > fs->buffer_length) {
    /* Oversize, allocate more */

    /* Sanity check on allocation size */
    if (new_min_length > FLEX_STRING_MAX_SIZE) {
      MOZ_CRASH();
    }

    /* Alloc to nearest chunk */
    fs->buffer_length = (((new_min_length - 1) / FLEX_STRING_CHUNK_SIZE) + 1) * FLEX_STRING_CHUNK_SIZE;

    fs->buffer = cpr_realloc(fs->buffer, fs->buffer_length);
  }
}

/*
 * flex_string_append
 *
 * Not thread-safe
 */
void flex_string_append(flex_string *fs, const char *more) {
  fs->string_length += strlen(more);

  flex_string_check_alloc(fs, fs->string_length + 1);

  sstrncat(fs->buffer, more, fs->buffer_length - strlen(fs->buffer));
}

/*
 * va_copy is part of the C99 spec but MSVC doesn't have it.
 */
#ifndef va_copy
#define va_copy(d,s) ((d) = (s))
#endif

/*
 * flex_string_vsprintf
 *
 * Not thread-safe
 */
void flex_string_vsprintf(flex_string *fs, const char *format, va_list original_ap) {
  va_list ap;
  int vsnprintf_result;

  va_copy(ap, original_ap);
  vsnprintf_result = vsnprintf(fs->buffer + fs->string_length, fs->buffer_length - fs->string_length, format, ap);
  va_end(ap);

  /* Special case just for Windows where vsnprintf is broken
     and returns -1 if buffer too large unless you size it 0. */
  if (vsnprintf_result < 0) {
    va_copy(ap, original_ap);
    vsnprintf_result = vsnprintf(NULL, 0, format, ap);
    va_end(ap);
  }

  if (fs->string_length + vsnprintf_result >= fs->buffer_length) {
    /* Buffer overflow, resize */
    flex_string_check_alloc(fs, fs->string_length + vsnprintf_result + 1);

    /* Try again with new buffer */
    va_copy(ap, original_ap);
    vsnprintf_result = vsnprintf(fs->buffer + fs->string_length, fs->buffer_length - fs->string_length, format, ap);
    va_end(ap);
    MOZ_ASSERT(vsnprintf_result > 0 &&
               (size_t)vsnprintf_result < (fs->buffer_length - fs->string_length));
  }

  if (vsnprintf_result > 0) {
    fs->string_length += vsnprintf_result;
  }
}

/*
 * flex_string_sprintf
 *
 * Not thread-safe
 */
void flex_string_sprintf(flex_string *fs, const char *format, ...) {
  va_list ap;

  va_start(ap, format);
  flex_string_vsprintf(fs, format, ap);
  va_end(ap);
}

