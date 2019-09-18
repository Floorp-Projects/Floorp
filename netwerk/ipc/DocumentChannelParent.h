/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DocumentChannelParent_h
#define mozilla_net_DocumentChannelParent_h

#include "mozilla/MozPromise.h"
#include "mozilla/Variant.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/net/PDocumentChannelParent.h"
#include "mozilla/net/ParentChannelListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIObserver.h"
#include "nsIParentChannel.h"
#include "nsIParentRedirectingChannel.h"
#include "nsIProcessSwitchRequestor.h"
#include "nsIRedirectResultListener.h"

#define DOCUMENT_CHANNEL_PARENT_IID                  \
  {                                                  \
    0x3b393c56, 0x9e01, 0x11e9, {                    \
      0xa2, 0xa3, 0x2a, 0x2a, 0xe2, 0xdb, 0xcc, 0xe4 \
    }                                                \
  }

namespace mozilla {
namespace net {

// TODO: We currently don't implement nsIProgressEventSink and forward those
// to the child. Should we? We get the interface requested.
class DocumentChannelParent : public nsIInterfaceRequestor,
                              public PDocumentChannelParent,
                              public nsIAsyncVerifyRedirectReadyCallback,
                              public nsIParentChannel,
                              public nsIChannelEventSink,
                              public HttpChannelSecurityWarningReporter,
                              public nsIProcessSwitchRequestor {
 public:
  explicit DocumentChannelParent(const dom::PBrowserOrId& iframeEmbedding,
                                 nsILoadContext* aLoadContext,
                                 PBOverrideStatus aOverrideStatus);

  bool Init(const DocumentChannelCreationArgs& aArgs);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIPARENTCHANNEL
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIASYNCVERIFYREDIRECTREADYCALLBACK
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIPROCESSSWITCHREQUESTOR

  NS_DECLARE_STATIC_IID_ACCESSOR(DOCUMENT_CHANNEL_PARENT_IID)

  bool RecvCancel(const nsresult& status);
  bool RecvSuspend();
  bool RecvResume();

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

  virtual void ActorDestroy(ActorDestroyReason why) override;

  // Notify the DocumentChannelChild that we're switching
  // to a different process and that it can notify listeners
  // that it's finished.
  void CancelChildForProcessSwitch();

 private:
  virtual ~DocumentChannelParent() = default;

  void TriggerRedirectToRealChannel(
      nsIChannel* aChannel,
      const Maybe<uint64_t>& aDestinationProcess = Nothing(),
      uint64_t aIdentifier = 0);

  void RedirectToRealChannelFinished(nsresult aRv);

  void FinishReplacementChannelSetup(bool aSucceeded);

  void TriggerCrossProcessSwitch();

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
  typedef mozilla::Variant<
      nsIHttpChannel::FlashPluginState, ClassifierMatchedInfoParams,
      ClassifierMatchedTrackingInfoParams, ClassificationFlagsParams>
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
  RefPtr<ParentChannelListener> mListener;

  nsCOMPtr<nsILoadContext> mLoadContext;

  PBOverrideStatus mPBOverride;

  // The original URI of the current channel. If there are redirects,
  // then the value on the channel gets overwritten with the original
  // URI of the first channel in the redirect chain, so we cache the
  // value we need here.
  nsCOMPtr<nsIURI> mChannelCreationURI;

  RefPtr<mozilla::dom::BrowserParent> mBrowserParent;

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

NS_DEFINE_STATIC_IID_ACCESSOR(DocumentChannelParent,
                              DOCUMENT_CHANNEL_PARENT_IID)

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_DocumentChannelParent_h
