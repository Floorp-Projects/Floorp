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
 * Define POSIX types
 *     [u]int[8,16,32,64]_t
 */
#include <stdint.h>

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

#endif // _CPR_WIN_TYPES_H_
