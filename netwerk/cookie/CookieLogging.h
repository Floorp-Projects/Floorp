/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieLogging_h
#define mozilla_net_CookieLogging_h

#include "mozilla/Logging.h"
#include "nsString.h"

class nsIURI;

namespace mozilla {
namespace net {

// define logging macros for convenience
#define SET_COOKIE true
#define GET_COOKIE false

extern LazyLogModule gCookieLog;

#define COOKIE_LOGFAILURE(a, b, c, d) CookieLogging::LogFailure(a, b, c, d)
#define COOKIE_LOGSUCCESS(a, b, c, d, e) \
  CookieLogging::LogSuccess(a, b, c, d, e)

#define COOKIE_LOGEVICTED(a, details)            \
  PR_BEGIN_MACRO                                 \
  if (MOZ_LOG_TEST(gCookieLog, LogLevel::Debug)) \
    CookieLogging::LogEvicted(a, details);       \
  PR_END_MACRO

#define COOKIE_LOGSTRING(lvl, fmt)  \
  PR_BEGIN_MACRO                    \
  MOZ_LOG(gCookieLog, lvl, fmt);    \
  MOZ_LOG(gCookieLog, lvl, ("\n")); \
  PR_END_MACRO

class Cookie;

class CookieLogging final {
 public:
  static void LogSuccess(bool aSetCookie, nsIURI* aHostURI,
                         const nsACString& aCookieString, Cookie* aCookie,
                         bool aReplacing);

  static void LogFailure(bool aSetCookie, nsIURI* aHostURI,
                         const nsACString& aCookieString, const char* aReason);

  static void LogCookie(Cookie* aCookie);

  static void LogEvicted(Cookie* aCookie, const char* aDetails);
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieLogging_h
