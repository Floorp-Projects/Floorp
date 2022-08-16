/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_nsCookieInjector_h__
#define mozilla_nsCookieInjector_h__

#include "nsCOMPtr.h"
#include "nsICookieBannerRule.h"
#include "nsIHttpChannel.h"
#include "nsIObserver.h"

namespace mozilla {

class nsCookieInjector final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static already_AddRefed<nsCookieInjector> GetSingleton();

  [[nodiscard]] nsresult Init();

  [[nodiscard]] nsresult Shutdown();

 private:
  nsCookieInjector() = default;
  ~nsCookieInjector() = default;

  // Whether the component is enabled and ready to inject cookies.
  bool mIsInitialized = false;

  // Enables or disables the component when the relevant prefs change.
  static void OnPrefChange(const char* aPref, void* aData);

  // Called when the http observer topic is dispatched.
  nsresult MaybeInjectCookies(nsIHttpChannel* aChannel, const char* aTopic);

  // Inserts cookies via the cookie manager given a list of cookie injection
  // rules.
  nsresult InjectCookiesFromRules(const nsCString& aHostPort,
                                  const nsTArray<RefPtr<nsICookieRule>>& aRules,
                                  OriginAttributes& aOriginAttributes);
};

}  // namespace mozilla

#endif
