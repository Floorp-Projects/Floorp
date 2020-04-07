/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LoadInfo_h
#define mozilla_LoadInfo_h

#include "nsIContentSecurityPolicy.h"
#include "nsILoadInfo.h"
#include "nsIPrincipal.h"
#include "nsIWeakReferenceUtils.h"  // for nsWeakPtr
#include "nsIURI.h"
#include "nsContentUtils.h"
#include "nsString.h"
#include "nsTArray.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"

class nsICookieJarSettings;
class nsINode;
class nsPIDOMWindowOuter;

namespace mozilla {

namespace dom {
class PerformanceStorage;
class XMLHttpRequestMainThread;
class CanonicalBrowsingContext;
}  // namespace dom

namespace net {
class LoadInfoArgs;
class LoadInfo;
}  // namespace net

namespace ipc {
// we have to forward declare that function so we can use it as a friend.
nsresult LoadInfoArgsToLoadInfo(
    const Maybe<mozilla::net::LoadInfoArgs>& aLoadInfoArgs,
    nsINode* aCspToInheritLoadingContext, net::LoadInfo** outLoadInfo);
}  // namespace ipc

namespace net {

typedef nsTArray<nsCOMPtr<nsIRedirectHistoryEntry>> RedirectHistoryArray;

/**
 * Class that provides an nsILoadInfo implementation.
 */
class LoadInfo final : public nsILoadInfo {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILOADINFO

  // aLoadingPrincipal MUST NOT BE NULL.
  LoadInfo(nsIPrincipal* aLoadingPrincipal, nsIPrincipal* aTriggeringPrincipal,
           nsINode* aLoadingContext, nsSecurityFlags aSecurityFlags,
           nsContentPolicyType aContentPolicyType,
           const Maybe<mozilla::dom::ClientInfo>& aLoadingClientInfo =
               Maybe<mozilla::dom::ClientInfo>(),
           const Maybe<mozilla::dom::ServiceWorkerDescriptor>& aController =
               Maybe<mozilla::dom::ServiceWorkerDescriptor>(),
           uint32_t aSandboxFlags = 0);

  // Constructor used for TYPE_DOCUMENT loads which have a different
  // loadingContext than other loads. This ContextForTopLevelLoad is
  // only used for content policy checks.
  LoadInfo(nsPIDOMWindowOuter* aOuterWindow, nsIPrincipal* aTriggeringPrincipal,
           nsISupports* aContextForTopLevelLoad, nsSecurityFlags aSecurityFlags,
           uint32_t aSandboxFlags);
  LoadInfo(dom::CanonicalBrowsingContext* aBrowsingContext,
           nsIPrincipal* aTriggeringPrincipal,
           const OriginAttributes& aOriginAttributes, uint64_t aOuterWindowID,
           nsSecurityFlags aSecurityFlags, uint32_t aSandboxFlags);

  // create an exact copy of the loadinfo
  already_AddRefed<nsILoadInfo> Clone() const;

  // hands off!!! don't use CloneWithNewSecFlags unless you know
  // exactly what you are doing - it should only be used within
  // nsBaseChannel::Redirect()
  already_AddRefed<nsILoadInfo> CloneWithNewSecFlags(
      nsSecurityFlags aSecurityFlags) const;
  // creates a copy of the loadinfo which is appropriate to use for a
  // separate request. I.e. not for a redirect or an inner channel, but
  // when a separate request is made with the same security properties.
  already_AddRefed<nsILoadInfo> CloneForNewRequest() const;

  void SetIsPreflight();
  void SetUpgradeInsecureRequests();
  void SetBrowserUpgradeInsecureRequests();
  void SetBrowserWouldUpgradeInsecureRequests();
  void SetIsFromProcessingFrameAttributes();

  // Hands off from the cspToInherit functionality!
  //
  // For navigations, GetCSPToInherit returns what the spec calls the
  // "request's client's global object's CSP list", or more precisely
  // a snapshot of it taken when the navigation starts.  For navigations
  // that need to inherit their CSP, this is the right CSP to use for
  // the new document.  We need a way to transfer the CSP from the
  // docshell (where the navigation starts) to the point where the new
  // document is created and decides whether to inherit its CSP, and
  // this is the mechanism we use for that.
  //
  // For example:
  // A document with a CSP triggers a new top-level data: URI load.
  // We pass the CSP of the document that triggered the load all the
  // way to docshell. Within docshell we call SetCSPToInherit() on the
  // loadinfo. Within Document::InitCSP() we check if the newly created
  // document needs to inherit the CSP. If so, we call GetCSPToInherit()
  // and set the inherited CSP as the CSP for the new document. Please
  // note that any additonal Meta CSP in that document will be merged
  // into that CSP. Any subresource loads within that document
  // subesquently will receive the correct CSP by querying
  // loadinfo->GetCSP() from that point on.
  void SetCSPToInherit(nsIContentSecurityPolicy* aCspToInherit) {
    mCspToInherit = aCspToInherit;
  }

 private:
  // private constructor that is only allowed to be called from within
  // HttpChannelParent and FTPChannelParent declared as friends undeneath.
  // In e10s we can not serialize nsINode, hence we store the innerWindowID.
  // Please note that aRedirectChain uses swapElements.
  LoadInfo(nsIPrincipal* aLoadingPrincipal, nsIPrincipal* aTriggeringPrincipal,
           nsIPrincipal* aPrincipalToInherit,
           nsIPrincipal* aSandboxedLoadingPrincipal,
           nsIPrincipal* aTopLevelPrincipal,
           nsIPrincipal* aTopLevelStorageAreaPrincipal,
           nsIURI* aResultPrincipalURI,
           nsICookieJarSettings* aCookieJarSettings,
           nsIContentSecurityPolicy* aCspToInherit,
           const Maybe<mozilla::dom::ClientInfo>& aClientInfo,
           const Maybe<mozilla::dom::ClientInfo>& aReservedClientInfo,
           const Maybe<mozilla::dom::ClientInfo>& aInitialClientInfo,
           const Maybe<mozilla::dom::ServiceWorkerDescriptor>& aController,
           nsSecurityFlags aSecurityFlags, uint32_t aSandboxFlags,
           nsContentPolicyType aContentPolicyType, LoadTainting aTainting,
           bool aBlockAllMixedContent, bool aUpgradeInsecureRequests,
           bool aBrowserUpgradeInsecureRequests,
           bool aBrowserWouldUpgradeInsecureRequests, bool aForceAllowDataURI,
           bool aAllowInsecureRedirectToDataURI, bool aBypassCORSChecks,
           bool aSkipContentPolicyCheckForWebRequest,
           bool aForceInheritPrincipalDropped, uint64_t aInnerWindowID,
           uint64_t aOuterWindowID, uint64_t aParentOuterWindowID,
           uint64_t aTopOuterWindowID, uint64_t aFrameOuterWindowID,
           uint64_t aBrowsingContextID, uint64_t aFrameBrowsingContextID,
           bool aInitialSecurityCheckDone, bool aIsThirdPartyRequest,
           bool aIsFormSubmission, bool aSendCSPViolationEvents,
           const OriginAttributes& aOriginAttributes,
           RedirectHistoryArray& aRedirectChainIncludingInternalRedirects,
           RedirectHistoryArray& aRedirectChain,
           nsTArray<nsCOMPtr<nsIPrincipal>>&& aAncestorPrincipals,
           const nsTArray<uint64_t>& aAncestorOuterWindowIDs,
           const nsTArray<nsCString>& aUnsafeHeaders, bool aForcePreflight,
           bool aIsPreflight, bool aLoadTriggeredFromExternal,
           bool aServiceWorkerTaintingSynthesized,
           bool aDocumentHasUserInteracted, bool aDocumentHasLoaded,
           bool aAllowListFutureDocumentsCreatedFromThisRedirectChain,
           const nsAString& aCspNonce, bool aSkipContentSniffing,
           uint32_t aHttpsOnlyStatus, bool aAllowDeprecatedSystemRequests,
           bool aHasStoragePermission, uint32_t aRequestBlockingReason,
           nsINode* aLoadingContext);
  LoadInfo(const LoadInfo& rhs);

  NS_IMETHOD GetRedirects(JSContext* aCx,
                          JS::MutableHandle<JS::Value> aRedirects,
                          const RedirectHistoryArray& aArra);

  friend nsresult mozilla::ipc::LoadInfoArgsToLoadInfo(
      const Maybe<mozilla::net::LoadInfoArgs>& aLoadInfoArgs,
      nsINode* aCspToInheritLoadingContext, net::LoadInfo** outLoadInfo);

  ~LoadInfo() = default;

  void ComputeIsThirdPartyContext(nsPIDOMWindowOuter* aOuterWindow);

  // This function is the *only* function which can change the securityflags
  // of a loadinfo. It only exists because of the XHR code. Don't call it
  // from anywhere else!
  void SetIncludeCookiesSecFlag();
  friend class mozilla::dom::XMLHttpRequestMainThread;

  // nsDocShell::OpenInitializedChannel needs to update the loadInfo with
  // the correct browsingContext.
  friend class ::nsDocShell;
  void UpdateBrowsingContextID(uint64_t aBrowsingContextID) {
    mBrowsingContextID = aBrowsingContextID;
  }
  void UpdateFrameBrowsingContextID(uint64_t aFrameBrowsingContextID) {
    mFrameBrowsingContextID = aFrameBrowsingContextID;
  }

  // if you add a member, please also update the copy constructor and consider
  // if it should be merged from parent channel through
  // ParentLoadInfoForwarderArgs.
  nsCOMPtr<nsIPrincipal> mLoadingPrincipal;
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
  nsCOMPtr<nsIPrincipal> mPrincipalToInherit;
  nsCOMPtr<nsIPrincipal> mSandboxedLoadingPrincipal;
  nsCOMPtr<nsIPrincipal> mTopLevelPrincipal;
  nsCOMPtr<nsIPrincipal> mTopLevelStorageAreaPrincipal;
  nsCOMPtr<nsIURI> mResultPrincipalURI;
  nsCOMPtr<nsICSPEventListener> mCSPEventListener;
  nsCOMPtr<nsICookieJarSettings> mCookieJarSettings;
  nsCOMPtr<nsIContentSecurityPolicy> mCspToInherit;

  Maybe<mozilla::dom::ClientInfo> mClientInfo;
  UniquePtr<mozilla::dom::ClientSource> mReservedClientSource;
  Maybe<mozilla::dom::ClientInfo> mReservedClientInfo;
  Maybe<mozilla::dom::ClientInfo> mInitialClientInfo;
  Maybe<mozilla::dom::ServiceWorkerDescriptor> mController;
  RefPtr<mozilla::dom::PerformanceStorage> mPerformanceStorage;

  nsWeakPtr mLoadingContext;
  nsWeakPtr mContextForTopLevelLoad;
  nsSecurityFlags mSecurityFlags;
  uint32_t mSandboxFlags;
  nsContentPolicyType mInternalContentPolicyType;
  LoadTainting mTainting;
  bool mBlockAllMixedContent;
  bool mUpgradeInsecureRequests;
  bool mBrowserUpgradeInsecureRequests;
  bool mBrowserWouldUpgradeInsecureRequests;
  bool mForceAllowDataURI;
  bool mAllowInsecureRedirectToDataURI;
  bool mBypassCORSChecks;
  bool mSkipContentPolicyCheckForWebRequest;
  bool mOriginalFrameSrcLoad;
  bool mForceInheritPrincipalDropped;
  uint64_t mInnerWindowID;
  uint64_t mOuterWindowID;
  uint64_t mParentOuterWindowID;
  uint64_t mTopOuterWindowID;
  uint64_t mFrameOuterWindowID;
  uint64_t mBrowsingContextID;
  uint64_t mFrameBrowsingContextID;
  bool mInitialSecurityCheckDone;
  bool mIsThirdPartyContext;
  bool mIsFormSubmission;
  bool mSendCSPViolationEvents;
  OriginAttributes mOriginAttributes;
  RedirectHistoryArray mRedirectChainIncludingInternalRedirects;
  RedirectHistoryArray mRedirectChain;
  nsTArray<nsCOMPtr<nsIPrincipal>> mAncestorPrincipals;
  nsTArray<uint64_t> mAncestorOuterWindowIDs;
  nsTArray<nsCString> mCorsUnsafeHeaders;
  uint32_t mRequestBlockingReason;
  bool mForcePreflight;
  bool mIsPreflight;
  bool mLoadTriggeredFromExternal;
  bool mServiceWorkerTaintingSynthesized;
  bool mDocumentHasUserInteracted;
  bool mDocumentHasLoaded;
  bool mAllowListFutureDocumentsCreatedFromThisRedirectChain;
  nsString mCspNonce;
  bool mSkipContentSniffing;
  uint32_t mHttpsOnlyStatus;
  bool mAllowDeprecatedSystemRequests;
  bool mHasStoragePermission;

  // Is true if this load was triggered by processing the attributes of the
  // browsing context container.
  // See nsILoadInfo.isFromProcessingFrameAttributes
  bool mIsFromProcessingFrameAttributes;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_LoadInfo_h
