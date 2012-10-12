/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_ASSERT_H_
#define _CPR_ASSERT_H_

#include "cpr_types.h"


__BEGIN_DECLS


#define __CPRSTRING(x) #x

extern void __cprAssert(const char *str, const char *file, int line);


__END_DECLS


#if defined SIP_OS_LINUX
#include "../linux/cpr_linux_assert.h"
#elif defined SIP_OS_WINDOWS
#include "../win32/cpr_win_assert.h"
#elif defined SIP_OS_OSX
#include "../darwin/cpr_darwin_assert.h"
#endif

#endif /* _CPR_ASSERT_H_ */

/*
 * This is outside the scope of the standard #ifdef <=> #endif header
 * file to allow a per-file definition.  By default, cprAssert is not
 * enabled.
 */
#undef cprAssert
#define cprAssert(expr, rc)

