/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_RAND_H_
#define _CPR_RAND_H_

__BEGIN_DECLS

#if defined SIP_OS_LINUX
#include "../linux/cpr_linux_rand.h"
#elif defined SIP_OS_WINDOWS
#include "../win32/cpr_win_rand.h"
#elif defined SIP_OS_OSX
#include "../darwin/cpr_darwin_rand.h"
#endif

__END_DECLS

#endif
