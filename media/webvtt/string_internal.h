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

# define UTF8_AMPERSAND         (0x26)
# define UTF8_LESS_THAN         (0x3C)
# define UTF8_GREATER_THAN      (0x3E)
# define UTF8_HYPHEN_MINUS      (0x2D)
# define UTF8_LEFT_TO_RIGHT_1   (0xE2)
# define UTF8_LEFT_TO_RIGHT_2   (0x80)
# define UTF8_LEFT_TO_RIGHT_3   (0x8E)
# define UTF8_RIGHT_TO_LEFT_1   (0xE2)
# define UTF8_RIGHT_TO_LEFT_2   (0x80)
# define UTF8_RIGHT_TO_LEFT_3   (0x8F)
# define UTF8_NO_BREAK_SPACE_1  (0xC2)
# define UTF8_NO_BREAK_SPACE_2  (0xA0)
# define UTF8_NULL_BYTE         (0x00)
# define UTF8_COLON             (0x3A)
# define UTF8_SEMI_COLON        (0x3B)
# define UTF8_TAB               (0x09)
# define UTF8_FORM_FEED         (0x0C)
# define UTF8_LINE_FEED         (0x0A)
# define UTF8_CARRIAGE_RETURN   (0x0D)
# define UTF8_FULL_STOP         (0x2E)
# define UTF8_SOLIDUS           (0x2F)
# define UTF8_SPACE             (0x20)
# define UTF8_DIGIT_ZERO        (0x30)
# define UTF8_DIGIT_NINE        (0x39)

# define UTF8_CAPITAL_A         (0x41)
# define UTF8_CAPITAL_Z         (0x5A)

# define UTF8_A                 (0x61)
# define UTF8_B                 (0x62)
# define UTF8_C                 (0x63)
# define UTF8_D                 (0x64)
# define UTF8_E                 (0x65)
# define UTF8_F                 (0x66)
# define UTF8_G                 (0x67)
# define UTF8_H                 (0x68)
# define UTF8_I                 (0x69)
# define UTF8_J                 (0x6A)
# define UTF8_K                 (0x6B)
# define UTF8_L                 (0x6C)
# define UTF8_M                 (0x6D)
# define UTF8_N                 (0x6E)
# define UTF8_O                 (0x6F)
# define UTF8_P                 (0x70)
# define UTF8_Q                 (0x71)
# define UTF8_R                 (0x72)
# define UTF8_S                 (0x73)
# define UTF8_T                 (0x74)
# define UTF8_U                 (0x75)
# define UTF8_V                 (0x76)
# define UTF8_W                 (0x77)
# define UTF8_X                 (0x78)
# define UTF8_Y                 (0x79)
# define UTF8_Z                 (0x7A)

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
  webvtt_byte *text;
  webvtt_byte array[1];
};

static __WEBVTT_STRING_INLINE  int
webvtt_isalpha( webvtt_byte ch )
{
  return ( ( ( ch >= UTF8_CAPITAL_A ) && ( ch <= UTF8_CAPITAL_Z ) ) || ( ( ch >= UTF8_A ) && ( ch <= UTF8_Z ) ) );
}
static __WEBVTT_STRING_INLINE int
webvtt_isdigit( webvtt_byte ch )
{
  return ( ( ch >= UTF8_DIGIT_ZERO ) && ( ch <= UTF8_DIGIT_NINE ) );
}

static __WEBVTT_STRING_INLINE int
webvtt_isalphanum( webvtt_byte ch )
{
  return ( webvtt_isalpha( ch ) || webvtt_isdigit( ch ) );
}

static __WEBVTT_STRING_INLINE int
webvtt_iswhite( webvtt_byte ch )
{
  return ( ( ch == UTF8_CARRIAGE_RETURN ) || ( ch == UTF8_LINE_FEED ) || ( ch == UTF8_FORM_FEED )
           || ( ch == UTF8_TAB ) || ( ch == UTF8_SPACE ) ) ;
}

# undef __WEBVTT_STRING_INLINE
#endif
