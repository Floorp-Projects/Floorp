/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CookieBannerDomainPrefService_h__
#define mozilla_CookieBannerDomainPrefService_h__

#include "nsIContentPrefService2.h"

#include "mozilla/Maybe.h"
#include "nsStringFwd.h"
#include "nsTHashMap.h"

#include "nsICookieBannerService.h"
#include "nsIObserver.h"

namespace mozilla {

// The service which maintains the per-domain cookie banner preference. It uses
// the content pref to store the per-domain preference for cookie banner
// handling. To support the synchronous access, the service caches the
// preferences in the memory.
class CookieBannerDomainPrefService final : public nsIContentPrefCallback2,
                                            public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPREFCALLBACK2
  NS_DECL_NSIOBSERVER

  static already_AddRefed<CookieBannerDomainPrefService> GetOrCreate();

  // Get the preference for the given domain.
  Maybe<nsICookieBannerService::Modes> GetPref(const nsACString& aDomain,
                                               bool aIsPrivate);

  // Set the preference for the given domain.
  [[nodiscard]] nsresult SetPref(const nsACString& aDomain,
                                 nsICookieBannerService::Modes aMode,
                                 bool aIsPrivate);

  // Remove the preference for the given domain.
  [[nodiscard]] nsresult RemovePref(const nsACString& aDomain, bool aIsPrivate);

  // Remove all site preferences.
  [[nodiscard]] nsresult RemoveAll(bool aIsPrivate);

  void Init();

 private:
  ~CookieBannerDomainPrefService() = default;

  CookieBannerDomainPrefService()
      : mIsInitialized(false), mIsContentPrefLoaded(false) {}

  // Indicates whether the service is initialized.
  bool mIsInitialized;

  // Indicates whether the first reading of content pref completed.
  bool mIsContentPrefLoaded;

  // Map of the per site preference keyed by domain.
  nsTHashMap<nsCStringHashKey, nsICookieBannerService::Modes> mPrefs;

  // Map of the per site preference for private windows keyed by domain.
  nsTHashMap<nsCStringHashKey, nsICookieBannerService::Modes> mPrefsPrivate;

  // A helper function that will wait until the initialization of the content
  // pref completed.
  void EnsureInitCompleted();

  void Shutdown();
};

}  // namespace mozilla

#endif  // mozilla_CookieBannerDomainPrefService_h__
