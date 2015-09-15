/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/LoadInfo.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsFrameLoader.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIFrameLoader.h"
#include "nsISupportsImpl.h"
#include "nsISupportsUtils.h"
#include "nsContentUtils.h"

namespace mozilla {

LoadInfo::LoadInfo(nsIPrincipal* aLoadingPrincipal,
                   nsIPrincipal* aTriggeringPrincipal,
                   nsINode* aLoadingContext,
                   nsSecurityFlags aSecurityFlags,
                   nsContentPolicyType aContentPolicyType)
  : mLoadingPrincipal(aLoadingContext ?
                        aLoadingContext->NodePrincipal() : aLoadingPrincipal)
  , mTriggeringPrincipal(aTriggeringPrincipal ?
                           aTriggeringPrincipal : mLoadingPrincipal.get())
  , mLoadingContext(do_GetWeakReference(aLoadingContext))
  , mSecurityFlags(aSecurityFlags)
  , mContentPolicyType(aContentPolicyType)
  , mUpgradeInsecureRequests(false)
  , mInnerWindowID(0)
  , mOuterWindowID(0)
  , mParentOuterWindowID(0)
  , mEnforceSecurity(false)
  , mInitialSecurityCheckDone(false)
{
  MOZ_ASSERT(mLoadingPrincipal);
  MOZ_ASSERT(mTriggeringPrincipal);

  // if consumers pass both, aLoadingContext and aLoadingPrincipal
  // then the loadingPrincipal must be the same as the node's principal
  MOZ_ASSERT(!aLoadingContext || !aLoadingPrincipal ||
             aLoadingContext->NodePrincipal() == aLoadingPrincipal);

  // if the load is sandboxed, we can not also inherit the principal
  if (mSecurityFlags & nsILoadInfo::SEC_SANDBOXED) {
    mSecurityFlags ^= nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL;
  }

  if (aLoadingContext) {
    nsCOMPtr<nsPIDOMWindow> outerWindow;

    // When the element being loaded is a frame, we choose the frame's window
    // for the window ID and the frame element's window as the parent
    // window. This is the behavior that Chrome exposes to add-ons.
    nsCOMPtr<nsIFrameLoaderOwner> frameLoaderOwner = do_QueryInterface(aLoadingContext);
    if (frameLoaderOwner) {
      nsCOMPtr<nsIFrameLoader> fl = frameLoaderOwner->GetFrameLoader();
      nsCOMPtr<nsIDocShell> docShell;
      if (fl && NS_SUCCEEDED(fl->GetDocShell(getter_AddRefs(docShell))) && docShell) {
        outerWindow = do_GetInterface(docShell);
      }
    } else {
      outerWindow = aLoadingContext->OwnerDoc()->GetWindow();
    }

    if (outerWindow) {
      nsCOMPtr<nsPIDOMWindow> inner = outerWindow->GetCurrentInnerWindow();
      mInnerWindowID = inner ? inner->WindowID() : 0;
      mOuterWindowID = outerWindow->WindowID();

      nsCOMPtr<nsIDOMWindow> parent;
      outerWindow->GetParent(getter_AddRefs(parent));
      nsCOMPtr<nsPIDOMWindow> piParent = do_QueryInterface(parent);
      mParentOuterWindowID = piParent->WindowID();
    }

    mUpgradeInsecureRequests = aLoadingContext->OwnerDoc()->GetUpgradeInsecureRequests();
  }
}

LoadInfo::LoadInfo(nsIPrincipal* aLoadingPrincipal,
                   nsIPrincipal* aTriggeringPrincipal,
                   nsSecurityFlags aSecurityFlags,
                   nsContentPolicyType aContentPolicyType,
                   bool aUpgradeInsecureRequests,
                   uint64_t aInnerWindowID,
                   uint64_t aOuterWindowID,
                   uint64_t aParentOuterWindowID,
                   bool aEnforceSecurity,
                   bool aInitialSecurityCheckDone,
                   nsTArray<nsCOMPtr<nsIPrincipal>>& aRedirectChain)
  : mLoadingPrincipal(aLoadingPrincipal)
  , mTriggeringPrincipal(aTriggeringPrincipal)
  , mSecurityFlags(aSecurityFlags)
  , mContentPolicyType(aContentPolicyType)
  , mUpgradeInsecureRequests(aUpgradeInsecureRequests)
  , mInnerWindowID(aInnerWindowID)
  , mOuterWindowID(aOuterWindowID)
  , mParentOuterWindowID(aParentOuterWindowID)
  , mEnforceSecurity(aEnforceSecurity)
  , mInitialSecurityCheckDone(aInitialSecurityCheckDone)
{
  MOZ_ASSERT(mLoadingPrincipal);
  MOZ_ASSERT(mTriggeringPrincipal);

  mRedirectChain.SwapElements(aRedirectChain);
}

LoadInfo::~LoadInfo()
{
}

NS_IMPL_ISUPPORTS(LoadInfo, nsILoadInfo)

NS_IMETHODIMP
LoadInfo::GetLoadingPrincipal(nsIPrincipal** aLoadingPrincipal)
{
  NS_ADDREF(*aLoadingPrincipal = mLoadingPrincipal);
  return NS_OK;
}

nsIPrincipal*
LoadInfo::LoadingPrincipal()
{
  return mLoadingPrincipal;
}

NS_IMETHODIMP
LoadInfo::GetTriggeringPrincipal(nsIPrincipal** aTriggeringPrincipal)
{
  NS_ADDREF(*aTriggeringPrincipal = mTriggeringPrincipal);
  return NS_OK;
}

nsIPrincipal*
LoadInfo::TriggeringPrincipal()
{
  return mTriggeringPrincipal;
}

NS_IMETHODIMP
LoadInfo::GetLoadingDocument(nsIDOMDocument** aResult)
{
  nsCOMPtr<nsINode> node = do_QueryReferent(mLoadingContext);
  if (node) {
    nsCOMPtr<nsIDOMDocument> context = do_QueryInterface(node->OwnerDoc());
    context.forget(aResult);
  }
  return NS_OK;
}

nsINode*
LoadInfo::LoadingNode()
{
  nsCOMPtr<nsINode> node = do_QueryReferent(mLoadingContext);
  return node;
}

NS_IMETHODIMP
LoadInfo::GetSecurityFlags(nsSecurityFlags* aResult)
{
  *aResult = mSecurityFlags;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetSecurityMode(uint32_t *aFlags)
{
  *aFlags = (mSecurityFlags &
              (nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS |
               nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED |
               nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS |
               nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL |
               nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS));
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetRequireCorsWithCredentials(bool* aResult)
{
  *aResult =
    (mSecurityFlags & nsILoadInfo::SEC_REQUIRE_CORS_WITH_CREDENTIALS);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetForceInheritPrincipal(bool* aInheritPrincipal)
{
  *aInheritPrincipal =
    (mSecurityFlags & nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetLoadingSandboxed(bool* aLoadingSandboxed)
{
  *aLoadingSandboxed = (mSecurityFlags & nsILoadInfo::SEC_SANDBOXED);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetAboutBlankInherits(bool* aResult)
{
  *aResult =
    (mSecurityFlags & nsILoadInfo::SEC_ABOUT_BLANK_INHERITS);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetAllowChrome(bool* aResult)
{
  *aResult =
    (mSecurityFlags & nsILoadInfo::SEC_ALLOW_CHROME);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetContentPolicyType(nsContentPolicyType* aResult)
{
  *aResult = nsContentUtils::InternalContentPolicyTypeToExternal(mContentPolicyType);
  return NS_OK;
}

nsContentPolicyType
LoadInfo::InternalContentPolicyType()
{
  return mContentPolicyType;
}

NS_IMETHODIMP
LoadInfo::GetUpgradeInsecureRequests(bool* aResult)
{
  *aResult = mUpgradeInsecureRequests;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetInnerWindowID(uint64_t* aResult)
{
  *aResult = mInnerWindowID;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetOuterWindowID(uint64_t* aResult)
{
  *aResult = mOuterWindowID;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetParentOuterWindowID(uint64_t* aResult)
{
  *aResult = mParentOuterWindowID;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetEnforceSecurity(bool aEnforceSecurity)
{
  // Indicates whether the channel was openend using AsyncOpen2. Once set
  // to true, it must remain true throughout the lifetime of the channel.
  // Setting it to anything else than true will be discarded.
  MOZ_ASSERT(aEnforceSecurity, "aEnforceSecurity must be true");
  mEnforceSecurity = mEnforceSecurity || aEnforceSecurity;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetEnforceSecurity(bool* aResult)
{
  *aResult = mEnforceSecurity;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetInitialSecurityCheckDone(bool aInitialSecurityCheckDone)
{
  // Indicates whether the channel was ever evaluated by the
  // ContentSecurityManager. Once set to true, this flag must
  // remain true throughout the lifetime of the channel.
  // Setting it to anything else than true will be discarded.
  MOZ_ASSERT(aInitialSecurityCheckDone, "aInitialSecurityCheckDone must be true");
  mInitialSecurityCheckDone = mInitialSecurityCheckDone || aInitialSecurityCheckDone;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetInitialSecurityCheckDone(bool* aResult)
{
  *aResult = mInitialSecurityCheckDone;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::AppendRedirectedPrincipal(nsIPrincipal* aPrincipal)
{
  NS_ENSURE_ARG(aPrincipal);
  mRedirectChain.AppendElement(aPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetRedirectChain(JSContext* aCx, JS::MutableHandle<JS::Value> aChain)
{
  if (!ToJSValue(aCx, mRedirectChain, aChain)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

const nsTArray<nsCOMPtr<nsIPrincipal>>&
LoadInfo::RedirectChain()
{
  return mRedirectChain;
}

} // namespace mozilla
