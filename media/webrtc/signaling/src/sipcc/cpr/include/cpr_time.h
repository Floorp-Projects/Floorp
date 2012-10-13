/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_TIME_H_
#define _CPR_TIME_H_

#include "cpr_types.h"

__BEGIN_DECLS

#if defined SIP_OS_LINUX
#include "../linux/cpr_linux_time.h"
#elif defined SIP_OS_WINDOWS
#include "../win32/cpr_win_time.h"
#elif defined SIP_OS_OSX
#include "../darwin/cpr_darwin_time.h"
#endif

/**
 * The generic "time" value strucutre
 */
struct cpr_timeval
{
    cpr_time_t    tv_sec;
    unsigned long tv_usec;
};

__END_DECLS

#endif
