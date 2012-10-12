/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_IN_H_
#define _CPR_IN_H_

#include "cpr_types.h"

__BEGIN_DECLS

#if defined SIP_OS_LINUX
#include "../linux/cpr_linux_in.h"
#elif defined SIP_OS_WINDOWS
#include "../win32/cpr_win_in.h"
#elif defined SIP_OS_OSX
#include "../darwin/cpr_darwin_in.h"
#endif

/**
 * Definitions for IP type of service. Used by pSIPCC for setting up different
 * TOS values.
 */
#define IP_TOS_LOWDELAY     0x10
#define IP_TOS_THROUGHPUT   0x08
#define IP_TOS_RELIABILITY  0x04

__END_DECLS

#endif
