/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_EarlyHintPreloader_h
#define mozilla_net_EarlyHintPreloader_h

#include "mozilla/Maybe.h"
#include "mozilla/PreloadHashKey.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIRedirectResultListener.h"
#include "nsIStreamListener.h"
#include "nsNetUtil.h"
#include "nsRefPtrHashtable.h"

class nsAttrValue;
class nsICookieJarSettings;
class nsIPrincipal;
class nsIReferrerInfo;

namespace mozilla::net {

class EarlyHintPreloader;
struct LinkHeader;

// class keeping track of all ongoing early hints
class OngoingEarlyHints final {
 public:
  NS_INLINE_DECL_REFCOUNTING(OngoingEarlyHints)
  MOZ_DECLARE_REFCOUNTED_TYPENAME(OngoingEarlyHints)

  OngoingEarlyHints() = default;

  // returns whether a preload with that key already existed
  bool Contains(const PreloadHashKey& aKey);
  bool Add(const PreloadHashKey& aKey, RefPtr<EarlyHintPreloader> aPreloader);

  void CancelAllOngoingPreloads();

 private:
  ~OngoingEarlyHints() = default;
  nsRefPtrHashtable<PreloadHashKey, EarlyHintPreloader> mOngoingPreloads;
};

class EarlyHintPreloader final : public nsIStreamListener,
                                 public nsIChannelEventSink,
                                 public nsIRedirectResultListener,
                                 public nsIInterfaceRequestor {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIREDIRECTRESULTLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR

 public:
  // Create and insert a preload into OngoingEarlyHints if the same preload
  // wasn't already issued and the LinkHeader can be parsed correctly.
  static void MaybeCreateAndInsertPreload(
      OngoingEarlyHints* aOngoingEarlyHints, const LinkHeader& aHeader,
      nsIURI* aBaseURI, nsIPrincipal* aTriggeringPrincipal,
      nsICookieJarSettings* aCookieJarSettings);

  // Should be called by the preloader service when the preload is not needed
  // after all, because the final response returns a non-2xx status code.
  nsresult CancelChannel(nsresult aStatus);

 private:
  explicit EarlyHintPreloader(nsIURI* aURI);
  ~EarlyHintPreloader() = default;

  static Maybe<PreloadHashKey> GenerateHashKey(ASDestination aAs, nsIURI* aURI,
                                               nsIPrincipal* aPrincipal,
                                               CORSMode corsMode,
                                               const nsAString& aType);

  static nsSecurityFlags ComputeSecurityFlags(CORSMode aCORSMode,
                                              ASDestination aAs,
                                              bool aIsModule);

  // call to start the preload
  nsresult OpenChannel(nsIPrincipal* aTriggeringPrincipal,
                       nsSecurityFlags aSecurityFlags,
                       nsContentPolicyType aContentPolicyType,
                       nsIReferrerInfo* aReferrerInfo,
                       nsICookieJarSettings* aCookieJarSettings);

  static void CollectResourcesTypeTelemetry(ASDestination aASDestination);
  // keep opening uri to not preload cross origins on redirects for now
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIChannel> mRedirectChannel;
};

inline nsISupports* ToSupports(EarlyHintPreloader* aObj) {
  return static_cast<nsIInterfaceRequestor*>(aObj);
}

}  // namespace mozilla::net

#endif  // mozilla_net_EarlyHintPreloader_h
