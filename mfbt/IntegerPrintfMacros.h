/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implements the C99 <inttypes.h> interface, minus the SCN* format macros. */

#ifndef mozilla_IntegerPrintfMacros_h_
#define mozilla_IntegerPrintfMacros_h_

/*
 * MSVC++ doesn't include <inttypes.h>, even in versions shipping <stdint.h>, so
 * we have to reimplement it there.  Note: <inttypes.h> #includes <stdint.h>.
 *
 * Note that this header DOES NOT implement <inttypes.h>'s scanf macros.  MSVC's
 * scanf doesn't have sufficient format specifier support to implement them
 * (specifically, to implement scanning into an 8-bit location).
 *
 * http://stackoverflow.com/questions/3036396/scanfd-char-char-as-int-format-string
 *
 * Moreover, scanf is a footgun: if the input number exceeds the bounds of the
 * target type, behavior is undefined (in the compiler sense: that is, this code
 * could overwrite your hard drive with zeroes):
 *
 *   uint8_t u;
 *   sscanf("256", "%" SCNu8, &u); // BAD
 *
 * This header will sometimes provide SCN* macros, by dint of being implemented
 * using <inttypes.h>.  But for these reasons, *never* use them!
 */

#if defined(MOZ_CUSTOM_INTTYPES_H)
#  include MOZ_CUSTOM_INTTYPES_H
#elif defined(_MSC_VER)
#  include "mozilla/MSIntTypes.h"
#else
#  include <inttypes.h>
#endif

/*
 * Fix up Android's broken [u]intptr_t inttype macros. Android's PRI*PTR
 * macros are defined as "ld", but sizeof(long) is 8 and sizeof(intptr_t)
 * is 4 on 32-bit Android. TestTypeTraits.cpp asserts that these new macro
 * definitions match the actual type sizes seen at compile time.
 */
#if defined(ANDROID) && !defined(__LP64__)
#  undef  PRIdPTR      /* intptr_t  */
#  define PRIdPTR "d"  /* intptr_t  */
#  undef  PRIiPTR      /* intptr_t  */
#  define PRIiPTR "i"  /* intptr_t  */
#  undef  PRIoPTR      /* uintptr_t */
#  define PRIoPTR "o"  /* uintptr_t */
#  undef  PRIuPTR      /* uintptr_t */
#  define PRIuPTR "u"  /* uintptr_t */
#  undef  PRIxPTR      /* uintptr_t */
#  define PRIxPTR "x"  /* uintptr_t */
#  undef  PRIXPTR      /* uintptr_t */
#  define PRIXPTR "X"  /* uintptr_t */
#endif

#endif  /* mozilla_IntegerPrintfMacros_h_ */
