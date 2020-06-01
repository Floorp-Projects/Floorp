/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieLogging.h"
#include "Cookie.h"

constexpr auto TIME_STRING_LENGTH = 40;

namespace mozilla {
namespace net {

LazyLogModule gCookieLog("cookie");

static const char* SameSiteToString(uint32_t aSameSite) {
  switch (aSameSite) {
    case nsICookie::SAMESITE_NONE:
      return "none";
    case nsICookie::SAMESITE_LAX:
      return "lax";
    case nsICookie::SAMESITE_STRICT:
      return "strict";
    default:
      MOZ_CRASH("Invalid nsICookie sameSite value");
      return "";
  }
}

// static
void CookieLogging::LogSuccess(bool aSetCookie, nsIURI* aHostURI,
                               const nsACString& aCookieString, Cookie* aCookie,
                               bool aReplacing) {
  // if logging isn't enabled, return now to save cycles
  if (!MOZ_LOG_TEST(gCookieLog, LogLevel::Debug)) {
    return;
  }

  nsAutoCString spec;
  if (aHostURI) {
    aHostURI->GetAsciiSpec(spec);
  }

  MOZ_LOG(gCookieLog, LogLevel::Debug,
          ("===== %s =====\n", aSetCookie ? "COOKIE ACCEPTED" : "COOKIE SENT"));
  MOZ_LOG(gCookieLog, LogLevel::Debug, ("request URL: %s\n", spec.get()));
  MOZ_LOG(gCookieLog, LogLevel::Debug,
          ("cookie string: %s\n", aCookieString.BeginReading()));
  if (aSetCookie) {
    MOZ_LOG(gCookieLog, LogLevel::Debug,
            ("replaces existing cookie: %s\n", aReplacing ? "true" : "false"));
  }

  LogCookie(aCookie);

  MOZ_LOG(gCookieLog, LogLevel::Debug, ("\n"));
}

// static
void CookieLogging::LogFailure(bool aSetCookie, nsIURI* aHostURI,
                               const nsACString& aCookieString,
                               const char* aReason) {
  // if logging isn't enabled, return now to save cycles
  if (!MOZ_LOG_TEST(gCookieLog, LogLevel::Warning)) {
    return;
  }

  nsAutoCString spec;
  if (aHostURI) {
    aHostURI->GetAsciiSpec(spec);
  }

  MOZ_LOG(gCookieLog, LogLevel::Warning,
          ("===== %s =====\n",
           aSetCookie ? "COOKIE NOT ACCEPTED" : "COOKIE NOT SENT"));
  MOZ_LOG(gCookieLog, LogLevel::Warning, ("request URL: %s\n", spec.get()));
  if (aSetCookie) {
    MOZ_LOG(gCookieLog, LogLevel::Warning,
            ("cookie string: %s\n", aCookieString.BeginReading()));
  }

  PRExplodedTime explodedTime;
  PR_ExplodeTime(PR_Now(), PR_GMTParameters, &explodedTime);
  char timeString[TIME_STRING_LENGTH];
  PR_FormatTimeUSEnglish(timeString, TIME_STRING_LENGTH, "%c GMT",
                         &explodedTime);

  MOZ_LOG(gCookieLog, LogLevel::Warning, ("current time: %s", timeString));
  MOZ_LOG(gCookieLog, LogLevel::Warning, ("rejected because %s\n", aReason));
  MOZ_LOG(gCookieLog, LogLevel::Warning, ("\n"));
}

// static
void CookieLogging::LogCookie(Cookie* aCookie) {
  PRExplodedTime explodedTime;
  PR_ExplodeTime(PR_Now(), PR_GMTParameters, &explodedTime);
  char timeString[TIME_STRING_LENGTH];
  PR_FormatTimeUSEnglish(timeString, TIME_STRING_LENGTH, "%c GMT",
                         &explodedTime);

  MOZ_LOG(gCookieLog, LogLevel::Debug, ("current time: %s", timeString));

  if (aCookie) {
    MOZ_LOG(gCookieLog, LogLevel::Debug, ("----------------\n"));
    MOZ_LOG(gCookieLog, LogLevel::Debug, ("name: %s\n", aCookie->Name().get()));
    MOZ_LOG(gCookieLog, LogLevel::Debug,
            ("value: %s\n", aCookie->Value().get()));
    MOZ_LOG(gCookieLog, LogLevel::Debug,
            ("%s: %s\n", aCookie->IsDomain() ? "domain" : "host",
             aCookie->Host().get()));
    MOZ_LOG(gCookieLog, LogLevel::Debug, ("path: %s\n", aCookie->Path().get()));

    PR_ExplodeTime(aCookie->Expiry() * int64_t(PR_USEC_PER_SEC),
                   PR_GMTParameters, &explodedTime);
    PR_FormatTimeUSEnglish(timeString, TIME_STRING_LENGTH, "%c GMT",
                           &explodedTime);
    MOZ_LOG(gCookieLog, LogLevel::Debug,
            ("expires: %s%s", timeString,
             aCookie->IsSession() ? " (at end of session)" : ""));

    PR_ExplodeTime(aCookie->CreationTime(), PR_GMTParameters, &explodedTime);
    PR_FormatTimeUSEnglish(timeString, TIME_STRING_LENGTH, "%c GMT",
                           &explodedTime);
    MOZ_LOG(gCookieLog, LogLevel::Debug, ("created: %s", timeString));

    MOZ_LOG(gCookieLog, LogLevel::Debug,
            ("is secure: %s\n", aCookie->IsSecure() ? "true" : "false"));
    MOZ_LOG(gCookieLog, LogLevel::Debug,
            ("is httpOnly: %s\n", aCookie->IsHttpOnly() ? "true" : "false"));
    MOZ_LOG(gCookieLog, LogLevel::Debug,
            ("sameSite: %s - rawSameSite: %s\n",
             SameSiteToString(aCookie->SameSite()),
             SameSiteToString(aCookie->RawSameSite())));

    nsAutoCString suffix;
    aCookie->OriginAttributesRef().CreateSuffix(suffix);
    MOZ_LOG(gCookieLog, LogLevel::Debug,
            ("origin attributes: %s\n",
             suffix.IsEmpty() ? "{empty}" : suffix.get()));
  }
}

// static
void CookieLogging::LogEvicted(Cookie* aCookie, const char* details) {
  MOZ_LOG(gCookieLog, LogLevel::Debug, ("===== COOKIE EVICTED =====\n"));
  MOZ_LOG(gCookieLog, LogLevel::Debug, ("%s\n", details));

  LogCookie(aCookie);

  MOZ_LOG(gCookieLog, LogLevel::Debug, ("\n"));
}

}  // namespace net
}  // namespace mozilla
