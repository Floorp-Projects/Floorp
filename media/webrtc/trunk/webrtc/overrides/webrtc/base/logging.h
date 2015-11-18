/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file overrides the logging macros in libjingle (webrtc/base/logging.h).
// Instead of using libjingle's logging implementation, the libjingle macros are
// mapped to the corresponding base/logging.h macro (chromium's VLOG).
// If this file is included outside of libjingle (e.g. in wrapper code) it
// should be included after base/logging.h (if any) or compiler error or
// unexpected behavior may occur (macros that have the same name in libjingle as
// in chromium will use the libjingle definition if this file is included
// first).

// Setting the LoggingSeverity (and lower) that should be written to file should
// be done via command line by specifying the flags:
// --vmodule or --v please see base/logging.h for details on how to use them.
// Specifying what file to write to is done using InitLogging also in
// base/logging.h.

// The macros and classes declared in here are not described as they are
// NOT TO BE USED outside of libjingle.

#ifndef THIRD_PARTY_LIBJINGLE_OVERRIDES_WEBRTC_BASE_LOGGING_H_
#define THIRD_PARTY_LIBJINGLE_OVERRIDES_WEBRTC_BASE_LOGGING_H_

#include "third_party/webrtc/overrides/webrtc/base/diagnostic_logging.h"

//////////////////////////////////////////////////////////////////////
// Libjingle macros which are mapped over to their VLOG equivalent in
// base/logging.h
//////////////////////////////////////////////////////////////////////

#if defined(LOGGING_INSIDE_WEBRTC)

#define DIAGNOSTIC_LOG(sev, ctx, err, ...) \
  rtc::DiagnosticLogMessage( \
      __FILE__, __LINE__, sev, VLOG_IS_ON(sev), \
      rtc::ERRCTX_ ## ctx, err, ##__VA_ARGS__).stream()

#define LOG_CHECK_LEVEL(sev) VLOG_IS_ON(rtc::sev)
#define LOG_CHECK_LEVEL_V(sev) VLOG_IS_ON(sev)

#define LOG_V(sev) DIAGNOSTIC_LOG(sev, NONE, 0)
#undef LOG
#define LOG(sev) DIAGNOSTIC_LOG(rtc::sev, NONE, 0)

// The _F version prefixes the message with the current function name.
#if defined(__GNUC__) && defined(_DEBUG)
#define LOG_F(sev) LOG(sev) << __PRETTY_FUNCTION__ << ": "
#else
#define LOG_F(sev) LOG(sev) << __FUNCTION__ << ": "
#endif

#define LOG_E(sev, ctx, err, ...) \
  DIAGNOSTIC_LOG(rtc::sev, ctx, err, ##__VA_ARGS__)

#undef LOG_ERRNO_EX
#define LOG_ERRNO_EX(sev, err) LOG_E(sev, ERRNO, err)
#undef LOG_ERRNO
#define LOG_ERRNO(sev) LOG_ERRNO_EX(sev, errno)

#if defined(WEBRTC_WIN)
#define LOG_GLE_EX(sev, err) LOG_E(sev, HRESULT, err)
#define LOG_GLE(sev) LOG_GLE_EX(sev, GetLastError())
#define LOG_GLEM(sev, mod) LOG_E(sev, HRESULT, GetLastError(), mod)
#define LOG_ERR_EX(sev, err) LOG_GLE_EX(sev, err)
#define LOG_ERR(sev) LOG_GLE(sev)
#define LAST_SYSTEM_ERROR (::GetLastError())
#else
#define LOG_ERR_EX(sev, err) LOG_ERRNO_EX(sev, err)
#define LOG_ERR(sev) LOG_ERRNO(sev)
#define LAST_SYSTEM_ERROR (errno)
#endif  // OS_WIN

#undef PLOG
#define PLOG(sev, err) LOG_ERR_EX(sev, err)

#endif  // LOGGING_INSIDE_WEBRTC

#endif  // THIRD_PARTY_LIBJINGLE_OVERRIDES_WEBRTC_BASE_LOGGING_H_
