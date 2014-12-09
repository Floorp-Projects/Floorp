/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_CNU_ERRNO_H_
#define _CPR_CNU_ERRNO_H_

#include <errno.h>

/* VC10 and above define these */
#ifndef EWOULDBLOCK
#define EWOULDBLOCK             WSAEWOULDBLOCK
#endif
#ifndef EINPROGRESS
#define EINPROGRESS             WSAEINPROGRESS
#endif
#ifndef ENOTCONN
#define ENOTCONN                WSAENOTCONN
#endif

/*
 * Maintain re-entrant nature by wrapping 'errno'
 */
#define cpr_errno cprTranslateErrno()

int16_t cprTranslateErrno(void);

#endif
