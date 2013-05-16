/**
 * Copyright (c) 2013 Mozilla Foundation and Contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __INTERN_STRING_H__
# define __INTERN_STRING_H__
# include <webvtt/string.h>

# define UTF8_LEFT_TO_RIGHT_1   (0xE2)
# define UTF8_LEFT_TO_RIGHT_2   (0x80)
# define UTF8_LEFT_TO_RIGHT_3   (0x8E)
# define UTF8_RIGHT_TO_LEFT_1   (0xE2)
# define UTF8_RIGHT_TO_LEFT_2   (0x80)
# define UTF8_RIGHT_TO_LEFT_3   (0x8F)
# define UTF8_NO_BREAK_SPACE_1  (0xC2)
# define UTF8_NO_BREAK_SPACE_2  (0xA0)

/**
 * Taken from ICU
 * http://source.icu-project.org/repos/icu/icu/trunk/source/common/unicode/utf.h
 */
# define UTF_IS_NONCHAR( C ) \
  ( ( C )>=0xFDD0 && \
  ( ( webvtt_uint32 )( C ) <= 0xfdef || ( ( C ) & 0xFFFE)==0xFFFE) && \
    ( webvtt_uint32 )( C ) <= 0x10FFFF )

# define UTF_HIGH_SURROGATE( C ) ( webvtt_uint16 )( ( ( C ) >> 10 ) + 0xD7C0 )
# define UTF_LOW_SURROGATE( C ) ( webvtt_uint16 )( ( ( C ) & 0x3FF ) | 0xDC00 )

# ifndef WEBVTT_MAX_LINE
#   define WEBVTT_MAX_LINE 0x10000
# endif

# ifdef WEBVTT_INLINE
#   define __WEBVTT_STRING_INLINE WEBVTT_INLINE
# else
#   define __WEBVTT_STRING_INLINE
# endif

struct
webvtt_string_data_t {
  struct webvtt_refcount_t refs;
  webvtt_uint32 alloc;
  webvtt_uint32 length;
  char *text;
  char array[1];
};

static __WEBVTT_STRING_INLINE  int
webvtt_isalpha( char ch )
{
  return ( ( ( ch >= 'A' ) && ( ch <= 'Z' ) ) || ( ( ch >= 'a' ) &&
                                                   ( ch <= 'z' ) ) );
}
static __WEBVTT_STRING_INLINE int
webvtt_isdigit( char ch )
{
  return ( ( ch >= '0' ) && ( ch <= '9' ) );
}

static __WEBVTT_STRING_INLINE int
webvtt_isalphanum( char ch )
{
  return ( webvtt_isalpha( ch ) || webvtt_isdigit( ch ) );
}

static __WEBVTT_STRING_INLINE int
webvtt_iswhite( char ch )
{
  return ( ( ch == '\r' ) || ( ch == '\n' ) || ( ch == '\f' )
           || ( ch == '\t' ) || ( ch == ' ' ) ) ;
}

# undef __WEBVTT_STRING_INLINE
#endif
