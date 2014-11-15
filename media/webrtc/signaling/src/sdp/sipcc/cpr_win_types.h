/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_WIN_TYPES_H_
#define _CPR_WIN_TYPES_H_

#include <sys/types.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#ifdef SIPCC_BUILD
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windef.h>
#endif
#include <stddef.h>
#include <stdlib.h>

/*
 * Determine the SDK we are using by making simplifying assumptions:
 * - If the compiler is Visual C, assume we are using the Microsoft SDK,
 * - If the compiler is GCC. assume we are using the MinGW W32API SDK.
 */
#ifdef _MSC_VER
#define CPR_WIN32_SDK_MICROSOFT
#endif
#ifdef __GNUC__
#define CPR_WIN32_SDK_MINGW
#endif

/*
 * Define POSIX types
 *     [u]int[8,16,32,64]_t
 *
 * The MinGW SDK has <stdint.h>, the Microsoft SDK has (or is it Visual C?):
 *     __int{8,16,32,64}.
 */
#if defined(CPR_WIN32_SDK_MINGW)
#include <stdint.h>
#elif defined(_MSC_VER) && defined(CPR_WIN32_SDK_MICROSOFT)
#if _MSC_VER >= 1600
#include <stdint.h>
#elif defined(CPR_STDINT_INCLUDE)
#include CPR_STDINT_INCLUDE
#else
typedef __int8  int8_t;
typedef __int16 int16_t;
typedef __int32 int32_t;
typedef __int64 int64_t;
typedef unsigned char  uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#endif
#endif

/*
 * Define boolean
 *     in windef.h: BOOL => int
 */
typedef uint8_t boolean;

/*
 * Define ssize_t if required.  The MinGW W32API already defines ssize_t
 * in <sys/types.h> (protected by _SSIZE_T_) so this will only apply to
 * Microsoft SDK.
 *
 * NOTE: size_t should already be declared by both the MinGW and Microsoft
 * SDKs.
 */
#ifndef _SSIZE_T_
#define _SSIZE_T_
typedef int ssize_t;
#endif

/*
 * Define pid_t.
 */
typedef int pid_t;

/*
 * Define min/max
 *    defined in windef.h as lowercase
 */
#ifndef MIN
#define MIN min
#endif

#ifndef MAX
#define MAX max
#endif

/*
 * Define NULL
 *    defined in numerous header files
 */
/* DONE defined in windef.h */

/*
 * Define NUL
 */
#ifndef NUL
#define NUL '\0'
#endif

/*
 * Define RESTRICT
 *
 * If supporting the ISO/IEC 9899:1999 standard,
 * use the 'restrict' keyword
 */
#if defined(_POSIX_C_SOURCE)
#define RESTRICT restrict
#else
#define RESTRICT
#endif

/*
 * Define CONST
 */
#ifndef CONST
#define CONST const
#endif

/*
 * Define __BEGIN_DECLS and __END_DECLS
 */
#ifdef __cplusplus
#define __BEGIN_DECLS extern "C" {
#define __END_DECLS   }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif

/* Not sure what this is, but we need it */
#define MSG_ECHO_EVENT 0xF001

/*
 * Define alternate punctuation token spellings
 * Added 'equals' which is not part of the standard iso646.h
 */
#ifndef __cplusplus
#include "iso646.h"
#endif
#define equals ==

#endif
