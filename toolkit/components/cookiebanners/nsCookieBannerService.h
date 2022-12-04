/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_nsCookieBannerService_h__
#define mozilla_nsCookieBannerService_h__

#include "nsICookieBannerRule.h"
#include "nsICookieBannerService.h"
#include "nsICookieBannerListService.h"
#include "nsCOMPtr.h"
#include "nsTHashMap.h"
#include "nsTHashSet.h"
#include "nsIObserver.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {

class CookieBannerDomainPrefService;

class nsCookieBannerService final : public nsIObserver,
                                    public nsICookieBannerService {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  NS_DECL_NSICOOKIEBANNERSERVICE

 public:
  static already_AddRefed<nsCookieBannerService> GetSingleton();

 private:
  nsCookieBannerService() = default;
  ~nsCookieBannerService() = default;

  // Whether the service is enabled and ready to accept requests.
  bool mIsInitialized = false;

  nsCOMPtr<nsICookieBannerListService> mListService;
  RefPtr<CookieBannerDomainPrefService> mDomainPrefService;

  // Map of site specific cookie banner rules keyed by domain.
  nsTHashMap<nsCStringHashKey, nsCOMPtr<nsICookieBannerRule>> mRules;

  // Map of global cookie banner rules keyed by id.
  nsTHashMap<nsCStringHashKey, nsCOMPtr<nsICookieBannerRule>> mGlobalRules;

  // Pref change callback which initializes and shuts down the service. This is
  // also called on startup.
  static void OnPrefChange(const char* aPref, void* aData);

  /**
   * Initializes internal state. Will be called on profile-after-change and on
   * pref changes.
   */
  [[nodiscard]] nsresult Init();

  /**
   * Cleanup method to be called on shutdown or pref change.
   */
  [[nodiscard]] nsresult Shutdown();

  nsresult GetRuleForDomain(const nsACString& aDomain, bool aIsTopLevel,
                            nsICookieBannerRule** aRule,
                            bool aReportTelemetry = false);

  nsresult GetRuleForURI(nsIURI* aURI, bool aIsTopLevel,
                         nsICookieBannerRule** aRule,
                         bool aReportTelemetry = false);

  void DailyReportTelemetry();

  // The hash sets of the domains that we have submitted telemetry. We use them
  // to report once for each domain.
  nsTHashSet<nsCStringHashKey> mTelemetryReportedTopDomains;
  nsTHashSet<nsCStringHashKey> mTelemetryReportedIFrameDomains;

  void ReportRuleLookupTelemetry(const nsACString& aDomain,
                                 nsICookieBannerRule* aRule, bool aIsTopLevel);
};

}  // namespace mozilla

#endif
