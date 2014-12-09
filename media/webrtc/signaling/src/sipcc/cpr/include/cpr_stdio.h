/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_STDIO_H_
#define _CPR_STDIO_H_

#include "cpr_types.h"

__BEGIN_DECLS

#if defined SIP_OS_LINUX
#include "../linux/cpr_linux_stdio.h"
#elif defined SIP_OS_WINDOWS
#include "../win32/cpr_win_stdio.h"
#elif defined SIP_OS_OSX
#include "../darwin/cpr_darwin_stdio.h"
#endif

/**
 * Debug message sent to syslog(#LOG_DEBUG)
 *
 * @param[in] _format  format string
 * @param[in] ...      variable arg list
 *
 * @return  Return code from vsnprintf
 *
 * @pre (_format not_eq NULL)
 */
extern int32_t buginf(const char *_format, ...);

/**
 * Debug message sent to syslog(#LOG_DEBUG) that can be larger than #LOG_MAX
 *
 * @param[in] str - a fixed constant string
 *
 * @return  zero(0)
 *
 * @pre (str not_eq NULL)
 */
extern int32_t buginf_msg(const char *str);

/**
 * Error message sent to syslog(#LOG_ERR)
 *
 * @param[in] _format  format string
 * @param[in] ...     variable arg list
 *
 * @return  Return code from vsnprintf
 *
 * @pre (_format not_eq NULL)
 */
extern void err_msg(const char *_format, ...);

/**
 * Notice message sent to syslog(#LOG_INFO)
 *
 * @param[in] _format  format string
 * @param[in] ...     variable arg list
 *
 * @return  Return code from vsnprintf
 *
 * @pre (_format not_eq NULL)
 */
extern void notice_msg(const char *_format, ...);

__END_DECLS

#endif

