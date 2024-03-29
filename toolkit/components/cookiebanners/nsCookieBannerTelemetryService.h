/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nsCookieBannerTelemetryService_h__
#define mozilla_nsCookieBannerTelemetryService_h__

#include "nsICookieBannerTelemetryService.h"

#include "nsIObserver.h"

class nsICookie;

namespace mozilla {
class nsCookieBannerTelemetryService final
    : public nsICookieBannerTelemetryService,
      public nsIObserver {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  NS_DECL_NSICOOKIEBANNERTELEMETRYSERVICE

 public:
  static already_AddRefed<nsCookieBannerTelemetryService> GetSingleton();

 private:
  nsCookieBannerTelemetryService() = default;
  ~nsCookieBannerTelemetryService() = default;

  bool mIsInitialized = false;
  bool mIsSearchServiceInitialized = false;

  [[nodiscard]] nsresult Init();

  [[nodiscard]] nsresult Shutdown();

  // Record the telemetry regarding the GDPR choice on Google Search domains.
  // We only record if the default search engine is Google. When passing no
  // cookie, we will walk through all cookies under Google Search domains.
  // Otherwise, we will report the GDPR choice according to the given cookie.
  [[nodiscard]] nsresult MaybeReportGoogleGDPRChoiceTelemetry(
      nsICookie* aCookie = nullptr, bool aReportEvent = false);
};

}  // namespace mozilla

#endif
