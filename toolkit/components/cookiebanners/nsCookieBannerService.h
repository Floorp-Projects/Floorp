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
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {

class CookieBannerDomainPrefService;

namespace dom {
class BrowsingContext;
}  // namespace dom

class nsCookieBannerService final : public nsIObserver,
                                    public nsICookieBannerService,
                                    public nsIWebProgressListener,
                                    public nsSupportsWeakReference {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIWEBPROGRESSLISTENER

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

  // The hash map to track if a top-level browsing context has either click
  // or cookie rule under its browsing context tree. We use the browsing context
  // id as the key. And the value is a tuple with two booleans that indicate
  // the existence of click rule and cookie rule respectively.
  nsTHashMap<uint64_t, std::tuple<bool, bool>> mReloadTelemetryData;

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

  nsresult GetClickRulesForDomainInternal(
      const nsACString& aDomain, const bool aIsTopLevel,
      const bool aReportTelemetry, nsTArray<RefPtr<nsIClickRule>>& aRules);

  nsresult GetCookieRulesForDomainInternal(
      const nsACString& aBaseDomain, const nsICookieBannerService::Modes aMode,
      const bool aIsTopLevel, const bool aReportTelemetry,
      nsTArray<RefPtr<nsICookieRule>>& aCookies);

  nsresult HasRuleForBrowsingContextInternal(
      mozilla::dom::BrowsingContext* aBrowsingContext, bool aIgnoreDomainPref,
      bool& aHasClickRule, bool& aHasCookieRule);

  nsresult GetRuleForDomain(const nsACString& aDomain, bool aIsTopLevel,
                            nsICookieBannerRule** aRule,
                            bool aReportTelemetry = false);

  /**
   * Lookup a domain pref by base domain.
   */
  nsresult GetDomainPrefInternal(const nsACString& aBaseDomain,
                                 const bool aIsPrivate,
                                 nsICookieBannerService::Modes* aModes);

  nsresult SetDomainPrefInternal(nsIURI* aTopLevelURI,
                                 nsICookieBannerService::Modes aModes,
                                 const bool aIsPrivate,
                                 const bool aPersistInPrivateBrowsing);

  /**
   * Get the rule matching the provided URI.
   * @param aURI - The URI to match the rule for.
   * @param aIsTopLevel - Whether this rule is requested for the top level frame
   * (true) or a child frame (false).
   * @param aRule - Rule to be populated
   * @param aDomain - Domain that matches the rule, computed from the URI.
   * @param aReportTelemetry - Whether telemetry should be recorded for this
   * call.
   * @returns The matching rule or nullptr if no matching rule is found.
   */
  nsresult GetRuleForURI(nsIURI* aURI, bool aIsTopLevel,
                         nsICookieBannerRule** aRule, nsACString& aDomain,
                         bool aReportTelemetry = false);

  nsresult GetServiceModeForBrowsingContext(
      dom::BrowsingContext* aBrowsingContext, bool aIgnoreDomainPref,
      nsICookieBannerService::Modes* aMode);

  nsresult RegisterWebProgressListener(nsISupports* aSubject);
  nsresult RemoveWebProgressListener(nsISupports* aSubject);

  void DailyReportTelemetry();

  // The hash sets of the domains that we have submitted telemetry. We use them
  // to report once for each domain.
  nsTHashSet<nsCStringHashKey> mTelemetryReportedTopDomains;
  nsTHashSet<nsCStringHashKey> mTelemetryReportedIFrameDomains;

  void ReportRuleLookupTelemetry(const nsACString& aDomain,
                                 nsICookieBannerRule* aRule, bool aIsTopLevel);

  // A record that stores whether we have executed the banner click for the
  // context.
  typedef struct ExecutedData {
    ExecutedData()
        : countExecutedInTop(0),
          countExecutedInFrame(0),
          countExecutedInTopPrivate(0),
          countExecutedInFramePrivate(0) {}

    uint8_t countExecutedInTop;
    uint8_t countExecutedInFrame;
    uint8_t countExecutedInTopPrivate;
    uint8_t countExecutedInFramePrivate;
  } ExecutedData;

  // Map of the sites (eTLD+1) that we have executed the cookie banner handling
  // for this session.
  nsTHashMap<nsCStringHashKey, ExecutedData> mExecutedDataForSites;
};

}  // namespace mozilla

#endif
