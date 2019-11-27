/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DocumentLoadListener_h
#define mozilla_net_DocumentLoadListener_h

#include "mozilla/MozPromise.h"
#include "mozilla/Variant.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/net/PDocumentChannelParent.h"
#include "mozilla/net/ParentChannelListener.h"
#include "mozilla/net/ADocumentChannelBridge.h"
#include "nsIInterfaceRequestor.h"
#include "nsIObserver.h"
#include "nsIParentChannel.h"
#include "nsIParentRedirectingChannel.h"
#include "nsIProcessSwitchRequestor.h"
#include "nsIRedirectResultListener.h"

#define DOCUMENT_LOAD_LISTENER_IID                   \
  {                                                  \
    0x3b393c56, 0x9e01, 0x11e9, {                    \
      0xa2, 0xa3, 0x2a, 0x2a, 0xe2, 0xdb, 0xcc, 0xe4 \
    }                                                \
  }

namespace mozilla {
namespace net {

/**
 * DocumentLoadListener represents a connecting document load for a
 * CanonicalBrowsingContext (in the parent process).
 *
 * It creates a network channel for the document load and then waits for it to
 * receive a response (after all redirects are resolved). It then decides where
 * to handle that load (could be in a different process from the initiator),
 * and then sets up a real network nsIChannel to deliver the data to the final
 * destination docshell, maybe through an nsIParentChannel/nsIChildChannel IPDL
 * layer.
 *
 * In the case where this was initiated from an nsDocShell, we also create an
 * nsIChannel to act as a placeholder within the docshell while this process
 * completes, and then notify the docshell of a 'redirect' when we replace this
 * channel with the real one.
 */

// TODO: We currently don't implement nsIProgressEventSink and forward those
// to the child. Should we? We get the interface requested.
class DocumentLoadListener : public nsIInterfaceRequestor,
                             public nsIAsyncVerifyRedirectReadyCallback,
                             public nsIParentChannel,
                             public nsIChannelEventSink,
                             public HttpChannelSecurityWarningReporter,
                             public nsIProcessSwitchRequestor {
 public:
  explicit DocumentLoadListener(const dom::PBrowserOrId& aIframeEmbedding,
                                nsILoadContext* aLoadContext,
                                PBOverrideStatus aOverrideStatus,
                                ADocumentChannelBridge* aBridge);

  // Creates the channel, and then calls AsyncOpen on it.
  bool Open(nsDocShellLoadState* aLoadState, class LoadInfo* aLoadInfo,
            const nsString* aInitiatorType, nsLoadFlags aLoadFlags,
            uint32_t aLoadType, uint32_t aCacheKey, bool aIsActive,
            bool aIsTopLevelDoc, bool aHasNonEmptySandboxingFlags,
            const Maybe<ipc::URIParams>& aTopWindowURI,
            const Maybe<ipc::PrincipalInfo>& aContentBlockingAllowListPrincipal,
            const nsString& aCustomUserAgent, const uint64_t& aChannelId,
            const TimeStamp& aAsyncOpenTime, nsresult* aRv);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIPARENTCHANNEL
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIASYNCVERIFYREDIRECTREADYCALLBACK
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIPROCESSSWITCHREQUESTOR

  // We suspend the underlying channel when replacing ourselves with
  // the real listener channel.
  // This helper resumes the underlying channel again, and manually
  // forwards any nsIStreamListener messages that arrived while we
  // were suspended (which might have failed).
  void ResumeSuspendedChannel(nsIStreamListener* aListener);

  NS_DECLARE_STATIC_IID_ACCESSOR(DOCUMENT_LOAD_LISTENER_IID)

  void Cancel(const nsresult& status);
  void Suspend();
  void Resume();

  nsresult ReportSecurityMessage(const nsAString& aMessageTag,
                                 const nsAString& aMessageCategory) override {
    ReportSecurityMessageParams params;
    params.mMessageTag = aMessageTag;
    params.mMessageCategory = aMessageCategory;
    mSecurityWarningFunctions.AppendElement(
        SecurityWarningFunction{VariantIndex<0>{}, std::move(params)});
    return NS_OK;
  }

  nsresult LogBlockedCORSRequest(const nsAString& aMessage,
                                 const nsACString& aCategory) override {
    LogBlockedCORSRequestParams params;
    params.mMessage = aMessage;
    params.mCategory = aCategory;
    mSecurityWarningFunctions.AppendElement(
        SecurityWarningFunction{VariantIndex<1>{}, std::move(params)});
    return NS_OK;
  }

  nsresult LogMimeTypeMismatch(const nsACString& aMessageName, bool aWarning,
                               const nsAString& aURL,
                               const nsAString& aContentType) override {
    LogMimeTypeMismatchParams params;
    params.mMessageName = aMessageName;
    params.mWarning = aWarning;
    params.mURL = aURL;
    params.mContentType = aContentType;
    mSecurityWarningFunctions.AppendElement(
        SecurityWarningFunction{VariantIndex<2>{}, std::move(params)});
    return NS_OK;
  }

  // Called by the bridge when it disconnects, so that we can drop
  // our reference to it.
  void DocumentChannelBridgeDisconnected();

  base::ProcessId OtherPid() const {
    if (mDocumentChannelBridge) {
      return mDocumentChannelBridge->OtherPid();
    }
    return 0;
  }

  bool AttachStreamFilter(
      ipc::Endpoint<mozilla::extensions::PStreamFilterParent>&& aEndpoint) {
    if (mDocumentChannelBridge) {
      return mDocumentChannelBridge->AttachStreamFilter(std::move(aEndpoint));
    }
    return false;
  }

  // Serializes all data needed to setup the new replacement channel
  // in the content process into the RedirectToRealChannelArgs struct.
  void SerializeRedirectData(RedirectToRealChannelArgs& aArgs,
                             bool aIsCrossProcess, uint32_t aRedirectFlags,
                             uint32_t aLoadFlags);

 protected:
  virtual ~DocumentLoadListener() = default;

  // Initiates the switch from DocumentChannel to the real protocol-specific
  // channel, and ensures that RedirectToRealChannelFinished is called when
  // this is complete.
  void TriggerRedirectToRealChannel(
      const Maybe<uint64_t>& aDestinationProcess = Nothing());

  // Called once the content-process side on setting up a replacement
  // channel is complete. May wait for the new parent channel to
  // finish, and then calls into FinishReplacementChannelSetup.
  void RedirectToRealChannelFinished(nsresult aRv);

  // Completes the replacement of the new channel.
  // This redirects the ParentChannelListener to forward any future
  // messages to the new channel, manually forwards any being held
  // by us, and resumes the underlying source channel.
  void FinishReplacementChannelSetup(bool aSucceeded);

  // Called when we have a cross-process switch promise. Waits on the
  // promise, and then call TriggerRedirectToRealChannel with the
  // provided content process id.
  void TriggerCrossProcessSwitch();

  // A helper for TriggerRedirectToRealChannel that abstracts over
  // the same-process and cross-process switch cases and returns
  // a single promise to wait on.
  RefPtr<PDocumentChannelParent::RedirectToRealChannelPromise>
  RedirectToRealChannel(uint32_t aRedirectFlags, uint32_t aLoadFlags,
                        const Maybe<uint64_t>& aDestinationProcess);

  // This defines a variant that describes all the attribute setters (and their
  // parameters) from nsIParentChannel
  //
  // NotifyFlashPluginStateChanged(nsIHttpChannel::FlashPluginState aState) = 0;
  // SetClassifierMatchedInfo(const nsACString& aList, const nsACString&
  // aProvider, const nsACString& aFullHash) = 0;
  // SetClassifierMatchedTrackingInfo(const nsACString& aLists, const
  // nsACString& aFullHashes) = 0; NotifyClassificationFlags(uint32_t
  // aClassificationFlags, bool aIsThirdParty) = 0;
  struct ClassifierMatchedInfoParams {
    nsCString mList;
    nsCString mProvider;
    nsCString mFullHash;
  };

  struct ClassifierMatchedTrackingInfoParams {
    nsCString mLists;
    nsCString mFullHashes;
  };

  struct ClassificationFlagsParams {
    uint32_t mClassificationFlags;
    bool mIsThirdParty;
  };

  struct NotifyChannelClassifierProtectionDisabledParams {
    uint32_t mAcceptedReason;
  };

  struct NotifyCookieAllowedParams {};

  struct NotifyCookieBlockedParams {
    uint32_t mRejectedReason;
  };

  typedef mozilla::Variant<
      nsIHttpChannel::FlashPluginState, ClassifierMatchedInfoParams,
      ClassifierMatchedTrackingInfoParams, ClassificationFlagsParams,
      NotifyChannelClassifierProtectionDisabledParams,
      NotifyCookieAllowedParams, NotifyCookieBlockedParams>
      IParentChannelFunction;

  // Store a list of all the attribute setters that have been called on this
  // channel, so that we can repeat them on the real channel that we redirect
  // to.
  nsTArray<IParentChannelFunction> mIParentChannelFunctions;

  // This defines a variant this describes all the functions
  // from HttpChannelSecurityWarningReporter so that we can forward
  // them on to the real channel.

  struct ReportSecurityMessageParams {
    nsString mMessageTag;
    nsString mMessageCategory;
  };

  struct LogBlockedCORSRequestParams {
    nsString mMessage;
    nsCString mCategory;
  };

  struct LogMimeTypeMismatchParams {
    nsCString mMessageName;
    bool mWarning;
    nsString mURL;
    nsString mContentType;
  };

  typedef mozilla::Variant<ReportSecurityMessageParams,
                           LogBlockedCORSRequestParams,
                           LogMimeTypeMismatchParams>
      SecurityWarningFunction;
  nsTArray<SecurityWarningFunction> mSecurityWarningFunctions;

  struct OnDataAvailableRequest {
    nsCString data;
    uint64_t offset;
    uint32_t count;
  };
  // TODO Backtrack this.
  nsTArray<OnDataAvailableRequest> mPendingRequests;
  Maybe<nsresult> mStopRequestValue;

  nsCOMPtr<nsIChannel> mChannel;

  // An instance of ParentChannelListener that we use as a listener
  // between mChannel (and any future redirected mChannels) and us.
  // This handles service worker interception, and retargetting
  // OnDataAvailable/OnStopRequest messages onto the listener that
  // replaces us.
  RefPtr<ParentChannelListener> mParentChannelListener;

  // The bridge to the nsIChannel in the originating docshell.
  // This reference forms a cycle with the bridge, and we expect
  // the bridge to call DisonnectDocumentChannelBridge when it
  // shuts down to break this.
  RefPtr<ADocumentChannelBridge> mDocumentChannelBridge;

  nsCOMPtr<nsILoadContext> mLoadContext;

  PBOverrideStatus mPBOverride;

  // The original URI of the current channel. If there are redirects,
  // then the value on the channel gets overwritten with the original
  // URI of the first channel in the redirect chain, so we cache the
  // value we need here.
  nsCOMPtr<nsIURI> mChannelCreationURI;

  nsTArray<DocumentChannelRedirect> mRedirects;

  // Corresponding redirect channel registrar Id for the final channel that
  // we want to use when redirecting the child, or doing a process switch.
  // 0 means redirection is not started.
  uint32_t mRedirectChannelId = 0;
  // Set to true if we called Suspend on mChannel to initiate our redirect.
  // This isn't currently set when we do a process swap, since that gets
  // initiated in nsHttpChannel.
  bool mSuspendedChannel = false;
  // Set to true if we're currently in the middle of replacing this with
  // a new channel connected a different process.
  bool mDoingProcessSwitch = false;
  // The value of GetApplyConversion on mChannel when OnStartRequest
  // was called. We override it to false to prevent a conversion
  // helper from being installed, but we need to restore the value
  // later.
  bool mOldApplyConversion = false;
  // Set to true if any previous channel that we redirected away
  // from had a COOP mismatch.
  bool mHasCrossOriginOpenerPolicyMismatch = false;

  typedef MozPromise<uint64_t, nsresult, true /* exclusive */>
      ContentProcessIdPromise;
  // This promise is set following a on-may-change-process observer
  // notification when the associated channel is getting relocated to another
  // process. It will be resolved when that process is set up.
  RefPtr<ContentProcessIdPromise> mRedirectContentProcessIdPromise;
  // This identifier is set at the same time as the
  // mRedirectContentProcessIdPromise.
  // This identifier is later passed to the childChannel in order to identify it
  // once the promise is resolved.
  uint64_t mCrossProcessRedirectIdentifier = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DocumentLoadListener, DOCUMENT_LOAD_LISTENER_IID)

inline nsISupports* ToSupports(DocumentLoadListener* aObj) {
  return static_cast<nsIInterfaceRequestor*>(aObj);
}

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_DocumentChannelParent_h
