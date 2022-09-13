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
#include "nsIObserver.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {

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

  nsresult GetRuleForDomain(const nsACString& aDomain,
                            nsICookieBannerRule** aRule);

  nsresult GetRuleForURI(nsIURI* aURI, nsICookieBannerRule** aRule);
};

}  // namespace mozilla

#endif
