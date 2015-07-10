/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/LoadInfo.h"

#include "mozilla/Assertions.h"
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
                   nsContentPolicyType aContentPolicyType,
                   nsIURI* aBaseURI)
  : mLoadingPrincipal(aLoadingContext ?
                        aLoadingContext->NodePrincipal() : aLoadingPrincipal)
  , mTriggeringPrincipal(aTriggeringPrincipal ?
                           aTriggeringPrincipal : mLoadingPrincipal.get())
  , mLoadingContext(do_GetWeakReference(aLoadingContext))
  , mSecurityFlags(aSecurityFlags)
  , mContentPolicyType(aContentPolicyType)
  , mBaseURI(aBaseURI)
  , mUpgradeInsecureRequests(false)
  , mInnerWindowID(0)
  , mOuterWindowID(0)
  , mParentOuterWindowID(0)
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
                   uint64_t aParentOuterWindowID)
  : mLoadingPrincipal(aLoadingPrincipal)
  , mTriggeringPrincipal(aTriggeringPrincipal)
  , mSecurityFlags(aSecurityFlags)
  , mContentPolicyType(aContentPolicyType)
  , mUpgradeInsecureRequests(aUpgradeInsecureRequests)
  , mInnerWindowID(aInnerWindowID)
  , mOuterWindowID(aOuterWindowID)
  , mParentOuterWindowID(aParentOuterWindowID)
{
  MOZ_ASSERT(mLoadingPrincipal);
  MOZ_ASSERT(mTriggeringPrincipal);
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
LoadInfo::GetBaseURI(nsIURI** aBaseURI)
{
  *aBaseURI = mBaseURI;
  NS_IF_ADDREF(*aBaseURI);
  return NS_OK;
}

nsIURI*
LoadInfo::BaseURI()
{
  return mBaseURI;
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

} // namespace mozilla
