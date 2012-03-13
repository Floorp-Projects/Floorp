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
typedef __int8  int8_t;
typedef __int16 int16_t;
typedef __int32 int32_t;
typedef __int64 int64_t;
typedef unsigned char  uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
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
