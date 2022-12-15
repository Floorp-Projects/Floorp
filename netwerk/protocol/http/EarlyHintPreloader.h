/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_EarlyHintPreloader_h
#define mozilla_net_EarlyHintPreloader_h

#include "mozilla/Maybe.h"
#include "mozilla/PreloadHashKey.h"
#include "NeckoCommon.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "nsHashtablesFwd.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIMultiPartChannel.h"
#include "nsIRedirectResultListener.h"
#include "nsIStreamListener.h"
#include "nsNetUtil.h"

class nsAttrValue;
class nsICookieJarSettings;
class nsIPrincipal;
class nsIReferrerInfo;

namespace mozilla::net {

class EarlyHintPreloader;
class EarlyHintConnectArgs;
class ParentChannelListener;
struct LinkHeader;

// class keeping track of all ongoing early hints
class OngoingEarlyHints final {
 public:
  NS_INLINE_DECL_REFCOUNTING(OngoingEarlyHints)

  OngoingEarlyHints() = default;

  // returns whether a preload with that key already existed
  bool Contains(const PreloadHashKey& aKey);
  bool Add(const PreloadHashKey& aKey, RefPtr<EarlyHintPreloader> aPreloader);

  void CancelAllOngoingPreloads(const nsACString& aReason);

  // registers all channels and returns the ids
  void RegisterLinksAndGetConnectArgs(
      nsTArray<EarlyHintConnectArgs>& aOutLinks);

 private:
  ~OngoingEarlyHints() = default;

  // We need to do two things requiring two separate variables to keep track of
  // preloads:
  //  - deduplicate Link headers when starting preloads, therefore we store them
  //    hashset with PreloadHashKey to look up whether we started the preload
  //    already
  //  - pass link headers in order they were received when passing all started
  //    preloads to the content process, therefore we store them in a nsTArray
  nsTHashSet<PreloadHashKey> mStartedPreloads;
  nsTArray<RefPtr<EarlyHintPreloader>> mPreloaders;
};

class EarlyHintPreloader final : public nsIStreamListener,
                                 public nsIChannelEventSink,
                                 public nsIRedirectResultListener,
                                 public nsIInterfaceRequestor,
                                 public nsIMultiPartChannelListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIREDIRECTRESULTLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIMULTIPARTCHANNELLISTENER

 public:
  // Create and insert a preload into OngoingEarlyHints if the same preload
  // wasn't already issued and the LinkHeader can be parsed correctly.
  static void MaybeCreateAndInsertPreload(
      OngoingEarlyHints* aOngoingEarlyHints, const LinkHeader& aHeader,
      nsIURI* aBaseURI, nsIPrincipal* aPrincipal,
      nsICookieJarSettings* aCookieJarSettings,
      const nsACString& aReferrerPolicy);

  // register Channel to EarlyHintRegistrar returns connect arguments
  EarlyHintConnectArgs Register();

  // Should be called by the preloader service when the preload is not
  // needed after all, because the final response returns a non-2xx status
  // code.
  nsresult CancelChannel(nsresult aStatus, const nsACString& aReason);

  void OnParentReady(nsIParentChannel* aParent, uint64_t aChannelId);

 private:
  void SetParentChannel();
  void InvokeStreamListenerFunctions();

  EarlyHintPreloader();
  ~EarlyHintPreloader();

  static Maybe<PreloadHashKey> GenerateHashKey(ASDestination aAs, nsIURI* aURI,
                                               nsIPrincipal* aPrincipal,
                                               CORSMode corsMode,
                                               const nsAString& aType);

  static nsSecurityFlags ComputeSecurityFlags(CORSMode aCORSMode,
                                              ASDestination aAs,
                                              bool aIsModule);

  // call to start the preload
  nsresult OpenChannel(nsIURI* aURI, nsIPrincipal* aPrincipal,
                       nsSecurityFlags aSecurityFlags,
                       nsContentPolicyType aContentPolicyType,
                       nsIReferrerInfo* aReferrerInfo,
                       nsICookieJarSettings* aCookieJarSettings);
  void PriorizeAsPreload();
  void SetLinkHeader(const LinkHeader& aLinkHeader);

  static void CollectResourcesTypeTelemetry(ASDestination aASDestination);
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIChannel> mRedirectChannel;
  uint64_t mChannelId = 0;

  EarlyHintConnectArgs mConnectArgs;

  // Copy behavior from DocumentLoadListener.h:
  // https://searchfox.org/mozilla-central/rev/c0bed29d643393af6ebe77aa31455f283f169202/netwerk/ipc/DocumentLoadListener.h#487-512
  // The set of nsIStreamListener functions that got called on this
  // listener, so that we can replay them onto the replacement channel's
  // listener. This should generally only be OnStartRequest, since we
  // Suspend() the channel at that point, but it can fail sometimes
  // so we have to support holding a list.
  nsTArray<StreamListenerFunction> mStreamListenerFunctions;

  // Set to true once OnStartRequest is called and we suspend our mChannel
  bool mSuspended = false;
  nsCOMPtr<nsIParentChannel> mParent;
  // Set to true after we've received the last OnStopRequest, and shouldn't
  // setup a reference from the ParentChannelListener to the replacement
  // channel.
  bool mIsFinished = false;

  RefPtr<ParentChannelListener> mParentListener;

 private:
  // IMPORTANT: when adding new values, always add them to the end, otherwise
  // it will mess up telemetry.
  enum EHPreloaderState : uint32_t {
    ePreloaderCreated = 0,
    ePreloaderOpened,
    ePreloaderUsed,
    ePreloaderCancelled,
  };
  EHPreloaderState mState = ePreloaderCreated;
  void SetState(EHPreloaderState aState) { mState = aState; }
};

inline nsISupports* ToSupports(EarlyHintPreloader* aObj) {
  return static_cast<nsIInterfaceRequestor*>(aObj);
}

}  // namespace mozilla::net

#endif  // mozilla_net_EarlyHintPreloader_h
