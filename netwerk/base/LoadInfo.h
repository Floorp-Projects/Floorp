/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LoadInfo_h
#define mozilla_LoadInfo_h

#include "nsIContentSecurityPolicy.h"
#include "nsIContentPolicy.h"
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

class nsICookieSettings;
class nsINode;
class nsPIDOMWindowOuter;

namespace mozilla {

namespace dom {
class PerformanceStorage;
class XMLHttpRequestMainThread;
}  // namespace dom

namespace net {
class LoadInfoArgs;
}  // namespace net

namespace ipc {
// we have to forward declare that function so we can use it as a friend.
nsresult LoadInfoArgsToLoadInfo(
    const Maybe<mozilla::net::LoadInfoArgs>& aLoadInfoArgs,
    nsILoadInfo** outLoadInfo);
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
               Maybe<mozilla::dom::ServiceWorkerDescriptor>());

  // Constructor used for TYPE_DOCUMENT loads which have a different
  // loadingContext than other loads. This ContextForTopLevelLoad is
  // only used for content policy checks.
  LoadInfo(nsPIDOMWindowOuter* aOuterWindow, nsIPrincipal* aTriggeringPrincipal,
           nsISupports* aContextForTopLevelLoad,
           nsSecurityFlags aSecurityFlags);

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
  // The only place that knows that a new document load needs to inherit the
  // CSP is the docshell. At that point neither the document nor the client
  // are available yet. Since we need a way to transfer the CSP from the
  // docshell to the document, we temporarily store the CSP that needs to
  // be inherited within the Loadinfo. In other words, those two functions
  // build the bridge to transfer the CSP from the docshell into the doc.
  void SetCSPToInherit(nsIContentSecurityPolicy* aCspToInherit) {
    mCspToInherit = aCspToInherit;
  }
  // Certain schemes need to inherit the CSP. If needed, we temporarily
  // store the CSP from the embedding/opening document here which then
  // gets propagated to the new doc within Document::InitCSP(). This
  // member is only ever non-null if the new doc actually needs to
  // inherit the CSP from the embedding/opening document.
  nsIContentSecurityPolicy* GetCSPToInherit() { return mCspToInherit; }

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
           nsIURI* aResultPrincipalURI, nsICookieSettings* aCookieSettings,
           nsIContentSecurityPolicy* aCspToInherit,
           const Maybe<mozilla::dom::ClientInfo>& aClientInfo,
           const Maybe<mozilla::dom::ClientInfo>& aReservedClientInfo,
           const Maybe<mozilla::dom::ClientInfo>& aInitialClientInfo,
           const Maybe<mozilla::dom::ServiceWorkerDescriptor>& aController,
           nsSecurityFlags aSecurityFlags,
           nsContentPolicyType aContentPolicyType, LoadTainting aTainting,
           bool aUpgradeInsecureRequests, bool aBrowserUpgradeInsecureRequests,
           bool aBrowserWouldUpgradeInsecureRequests, bool aForceAllowDataURI,
           bool aAllowInsecureRedirectToDataURI,
           bool aSkipContentPolicyCheckForWebRequest,
           bool aForceInheritPrincipalDropped, uint64_t aInnerWindowID,
           uint64_t aOuterWindowID, uint64_t aParentOuterWindowID,
           uint64_t aTopOuterWindowID, uint64_t aFrameOuterWindowID,
           uint64_t aBrowsingContextID, uint64_t aFrameBrowsingContextID,
           bool aInitialSecurityCheckDone, bool aIsThirdPartyRequest,
           bool aIsDocshellReload, bool aSendCSPViolationEvents,
           const OriginAttributes& aOriginAttributes,
           RedirectHistoryArray& aRedirectChainIncludingInternalRedirects,
           RedirectHistoryArray& aRedirectChain,
           nsTArray<nsCOMPtr<nsIPrincipal>>&& aAncestorPrincipals,
           const nsTArray<uint64_t>& aAncestorOuterWindowIDs,
           const nsTArray<nsCString>& aUnsafeHeaders, bool aForcePreflight,
           bool aIsPreflight, bool aLoadTriggeredFromExternal,
           bool aServiceWorkerTaintingSynthesized,
           bool aDocumentHasUserInteracted, bool aDocumentHasLoaded,
           const nsAString& aCspNonce, uint32_t aRequestBlockingReason);
  LoadInfo(const LoadInfo& rhs);

  NS_IMETHOD GetRedirects(JSContext* aCx,
                          JS::MutableHandle<JS::Value> aRedirects,
                          const RedirectHistoryArray& aArra);

  friend nsresult mozilla::ipc::LoadInfoArgsToLoadInfo(
      const Maybe<mozilla::net::LoadInfoArgs>& aLoadInfoArgs,
      nsILoadInfo** outLoadInfo);

  ~LoadInfo() = default;

  void ComputeIsThirdPartyContext(nsPIDOMWindowOuter* aOuterWindow);

  // This function is the *only* function which can change the securityflags
  // of a loadinfo. It only exists because of the XHR code. Don't call it
  // from anywhere else!
  void SetIncludeCookiesSecFlag();
  friend class mozilla::dom::XMLHttpRequestMainThread;

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
  nsCOMPtr<nsICookieSettings> mCookieSettings;
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
  nsContentPolicyType mInternalContentPolicyType;
  LoadTainting mTainting;
  bool mUpgradeInsecureRequests;
  bool mBrowserUpgradeInsecureRequests;
  bool mBrowserWouldUpgradeInsecureRequests;
  bool mForceAllowDataURI;
  bool mAllowInsecureRedirectToDataURI;
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
  bool mIsDocshellReload;
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
  nsString mCspNonce;

  // Is true if this load was triggered by processing the attributes of the
  // browsing context container.
  // See nsILoadInfo.isFromProcessingFrameAttributes
  bool mIsFromProcessingFrameAttributes;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_LoadInfo_h
