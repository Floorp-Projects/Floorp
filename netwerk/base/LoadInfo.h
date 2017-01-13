/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LoadInfo_h
#define mozilla_LoadInfo_h

#include "nsIContentPolicy.h"
#include "nsILoadInfo.h"
#include "nsIPrincipal.h"
#include "nsIWeakReferenceUtils.h" // for nsWeakPtr
#include "nsIURI.h"
#include "nsTArray.h"

#include "mozilla/BasePrincipal.h"

class nsINode;
class nsPIDOMWindowOuter;

namespace mozilla {

namespace dom {
class XMLHttpRequestMainThread;
}

namespace net {
class OptionalLoadInfoArgs;
} // namespace net

namespace ipc {
// we have to forward declare that function so we can use it as a friend.
nsresult
LoadInfoArgsToLoadInfo(const mozilla::net::OptionalLoadInfoArgs& aLoadInfoArgs,
                       nsILoadInfo** outLoadInfo);
} // namespace ipc

namespace net {

/**
 * Class that provides an nsILoadInfo implementation.
 *
 * Note that there is no reason why this class should be MOZ_EXPORT, but
 * Thunderbird relies on some insane hacks which require this, so we'll leave it
 * as is for now, but hopefully we'll be able to remove the MOZ_EXPORT keyword
 * from this class at some point.  See bug 1149127 for the discussion.
 */
class MOZ_EXPORT LoadInfo final : public nsILoadInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILOADINFO

  // aLoadingPrincipal MUST NOT BE NULL.
  LoadInfo(nsIPrincipal* aLoadingPrincipal,
           nsIPrincipal* aTriggeringPrincipal,
           nsINode* aLoadingContext,
           nsSecurityFlags aSecurityFlags,
           nsContentPolicyType aContentPolicyType);

  // Constructor used for TYPE_DOCUMENT loads which have no reasonable
  // loadingNode or loadingPrincipal
  LoadInfo(nsPIDOMWindowOuter* aOuterWindow,
           nsIPrincipal* aTriggeringPrincipal,
           nsSecurityFlags aSecurityFlags);

  // create an exact copy of the loadinfo
  already_AddRefed<nsILoadInfo> Clone() const;
  // hands off!!! don't use CloneWithNewSecFlags unless you know
  // exactly what you are doing - it should only be used within
  // nsBaseChannel::Redirect()
  already_AddRefed<nsILoadInfo>
  CloneWithNewSecFlags(nsSecurityFlags aSecurityFlags) const;
  // creates a copy of the loadinfo which is appropriate to use for a
  // separate request. I.e. not for a redirect or an inner channel, but
  // when a separate request is made with the same security properties.
  already_AddRefed<nsILoadInfo> CloneForNewRequest() const;

  void SetIsPreflight();

private:
  // private constructor that is only allowed to be called from within
  // HttpChannelParent and FTPChannelParent declared as friends undeneath.
  // In e10s we can not serialize nsINode, hence we store the innerWindowID.
  // Please note that aRedirectChain uses swapElements.
  LoadInfo(nsIPrincipal* aLoadingPrincipal,
           nsIPrincipal* aTriggeringPrincipal,
           nsIPrincipal* aPrincipalToInherit,
           nsSecurityFlags aSecurityFlags,
           nsContentPolicyType aContentPolicyType,
           LoadTainting aTainting,
           bool aUpgradeInsecureRequests,
           bool aVerifySignedContent,
           bool aEnforceSRI,
           bool aForceInheritPrincipalDropped,
           uint64_t aInnerWindowID,
           uint64_t aOuterWindowID,
           uint64_t aParentOuterWindowID,
           uint64_t aFrameOuterWindowID,
           bool aEnforceSecurity,
           bool aInitialSecurityCheckDone,
           bool aIsThirdPartyRequest,
           const OriginAttributes& aOriginAttributes,
           nsTArray<nsCOMPtr<nsIPrincipal>>& aRedirectChainIncludingInternalRedirects,
           nsTArray<nsCOMPtr<nsIPrincipal>>& aRedirectChain,
           const nsTArray<nsCString>& aUnsafeHeaders,
           bool aForcePreflight,
           bool aIsPreflight,
           bool aForceHSTSPriming,
           bool aMixedContentWouldBlock);
  LoadInfo(const LoadInfo& rhs);

  friend nsresult
  mozilla::ipc::LoadInfoArgsToLoadInfo(
    const mozilla::net::OptionalLoadInfoArgs& aLoadInfoArgs,
    nsILoadInfo** outLoadInfo);

  ~LoadInfo();

  void ComputeIsThirdPartyContext(nsPIDOMWindowOuter* aOuterWindow);

  // This function is the *only* function which can change the securityflags
  // of a loadinfo. It only exists because of the XHR code. Don't call it
  // from anywhere else!
  void SetIncludeCookiesSecFlag();
  friend class mozilla::dom::XMLHttpRequestMainThread;

  // if you add a member, please also update the copy constructor
  nsCOMPtr<nsIPrincipal>           mLoadingPrincipal;
  nsCOMPtr<nsIPrincipal>           mTriggeringPrincipal;
  nsCOMPtr<nsIPrincipal>           mPrincipalToInherit;
  nsWeakPtr                        mLoadingContext;
  nsSecurityFlags                  mSecurityFlags;
  nsContentPolicyType              mInternalContentPolicyType;
  LoadTainting                     mTainting;
  bool                             mUpgradeInsecureRequests;
  bool                             mVerifySignedContent;
  bool                             mEnforceSRI;
  bool                             mForceInheritPrincipalDropped;
  uint64_t                         mInnerWindowID;
  uint64_t                         mOuterWindowID;
  uint64_t                         mParentOuterWindowID;
  uint64_t                         mFrameOuterWindowID;
  bool                             mEnforceSecurity;
  bool                             mInitialSecurityCheckDone;
  bool                             mIsThirdPartyContext;
  OriginAttributes                 mOriginAttributes;
  nsTArray<nsCOMPtr<nsIPrincipal>> mRedirectChainIncludingInternalRedirects;
  nsTArray<nsCOMPtr<nsIPrincipal>> mRedirectChain;
  nsTArray<nsCString>              mCorsUnsafeHeaders;
  bool                             mForcePreflight;
  bool                             mIsPreflight;

  bool                             mForceHSTSPriming : 1;
  bool                             mMixedContentWouldBlock : 1;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_LoadInfo_h

