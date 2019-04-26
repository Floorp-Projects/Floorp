/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/LoadInfo.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/ClientSource.h"
#include "mozilla/dom/PerformanceStorage.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/net/CookieSettings.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/StaticPrefs.h"
#include "mozIThirdPartyUtil.h"
#include "nsFrameLoader.h"
#include "nsFrameLoaderOwner.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIDocShell.h"
#include "mozilla/dom/Document.h"
#include "nsCookiePermission.h"
#include "nsICookieService.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsISupportsImpl.h"
#include "nsISupportsUtils.h"
#include "nsIXPConnect.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsGlobalWindow.h"
#include "nsMixedContentBlocker.h"
#include "nsQueryObject.h"
#include "nsRedirectHistoryEntry.h"
#include "nsSandboxFlags.h"
#include "LoadInfo.h"

using namespace mozilla::dom;

namespace mozilla {
namespace net {

static uint64_t FindTopOuterWindowID(nsPIDOMWindowOuter* aOuter) {
  nsCOMPtr<nsPIDOMWindowOuter> outer = aOuter;
  while (nsCOMPtr<nsPIDOMWindowOuter> parent =
             outer->GetScriptableParentOrNull()) {
    outer = parent;
  }
  return outer->WindowID();
}

LoadInfo::LoadInfo(
    nsIPrincipal* aLoadingPrincipal, nsIPrincipal* aTriggeringPrincipal,
    nsINode* aLoadingContext, nsSecurityFlags aSecurityFlags,
    nsContentPolicyType aContentPolicyType,
    const Maybe<mozilla::dom::ClientInfo>& aLoadingClientInfo,
    const Maybe<mozilla::dom::ServiceWorkerDescriptor>& aController)
    : mLoadingPrincipal(aLoadingContext ? aLoadingContext->NodePrincipal()
                                        : aLoadingPrincipal),
      mTriggeringPrincipal(aTriggeringPrincipal ? aTriggeringPrincipal
                                                : mLoadingPrincipal.get()),
      mPrincipalToInherit(nullptr),
      mClientInfo(aLoadingClientInfo),
      mController(aController),
      mLoadingContext(do_GetWeakReference(aLoadingContext)),
      mContextForTopLevelLoad(nullptr),
      mSecurityFlags(aSecurityFlags),
      mInternalContentPolicyType(aContentPolicyType),
      mTainting(LoadTainting::Basic),
      mUpgradeInsecureRequests(false),
      mBrowserUpgradeInsecureRequests(false),
      mBrowserWouldUpgradeInsecureRequests(false),
      mForceAllowDataURI(false),
      mAllowInsecureRedirectToDataURI(false),
      mSkipContentPolicyCheckForWebRequest(false),
      mOriginalFrameSrcLoad(false),
      mForceInheritPrincipalDropped(false),
      mInnerWindowID(0),
      mOuterWindowID(0),
      mParentOuterWindowID(0),
      mTopOuterWindowID(0),
      mFrameOuterWindowID(0),
      mBrowsingContextID(0),
      mFrameBrowsingContextID(0),
      mInitialSecurityCheckDone(false),
      mIsThirdPartyContext(false),
      mIsDocshellReload(false),
      mSendCSPViolationEvents(true),
      mRequestBlockingReason(BLOCKING_REASON_NONE),
      mForcePreflight(false),
      mIsPreflight(false),
      mLoadTriggeredFromExternal(false),
      mServiceWorkerTaintingSynthesized(false),
      mDocumentHasUserInteracted(false),
      mDocumentHasLoaded(false),
      mIsFromProcessingFrameAttributes(false) {
  MOZ_ASSERT(mLoadingPrincipal);
  MOZ_ASSERT(mTriggeringPrincipal);

#ifdef DEBUG
  // TYPE_DOCUMENT loads initiated by javascript tests will go through
  // nsIOService and use the wrong constructor.  Don't enforce the
  // !TYPE_DOCUMENT check in those cases
  bool skipContentTypeCheck = false;
  skipContentTypeCheck =
      Preferences::GetBool("network.loadinfo.skip_type_assertion");
#endif

  // This constructor shouldn't be used for TYPE_DOCUMENT loads that don't
  // have a loadingPrincipal
  MOZ_ASSERT(skipContentTypeCheck || mLoadingPrincipal ||
             mInternalContentPolicyType != nsIContentPolicy::TYPE_DOCUMENT);

  // We should only get an explicit controller for subresource requests.
  MOZ_DIAGNOSTIC_ASSERT(aController.isNothing() ||
                        !nsContentUtils::IsNonSubresourceInternalPolicyType(
                            mInternalContentPolicyType));

  // TODO(bug 1259873): Above, we initialize mIsThirdPartyContext to false
  // meaning that consumers of LoadInfo that don't pass a context or pass a
  // context from which we can't find a window will default to assuming that
  // they're 1st party. It would be nice if we could default "safe" and assume
  // that we are 3rd party until proven otherwise.

  // if consumers pass both, aLoadingContext and aLoadingPrincipal
  // then the loadingPrincipal must be the same as the node's principal
  MOZ_ASSERT(!aLoadingContext || !aLoadingPrincipal ||
             aLoadingContext->NodePrincipal() == aLoadingPrincipal);

  // if the load is sandboxed, we can not also inherit the principal
  if (mSecurityFlags & nsILoadInfo::SEC_SANDBOXED) {
    mForceInheritPrincipalDropped =
        (mSecurityFlags & nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL);
    mSecurityFlags &= ~nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL;
  }

  uint32_t externalType =
      nsContentUtils::InternalContentPolicyTypeToExternal(aContentPolicyType);

  if (aLoadingContext) {
    // Ensure that all network requests for a window client have the ClientInfo
    // properly set.  Workers must currently pass the loading ClientInfo
    // explicitly. We allow main thread requests to explicitly pass the value as
    // well.
    if (mClientInfo.isNothing()) {
      mClientInfo = aLoadingContext->OwnerDoc()->GetClientInfo();
    }

    // For subresource loads set the service worker based on the calling
    // context's controller.  Workers must currently pass the controller in
    // explicitly.  We allow main thread requests to explicitly pass the value
    // as well, but otherwise extract from the loading context here.
    if (mController.isNothing() &&
        !nsContentUtils::IsNonSubresourceInternalPolicyType(
            mInternalContentPolicyType)) {
      mController = aLoadingContext->OwnerDoc()->GetController();
    }

    nsCOMPtr<nsPIDOMWindowOuter> contextOuter =
        aLoadingContext->OwnerDoc()->GetWindow();
    if (contextOuter) {
      ComputeIsThirdPartyContext(contextOuter);
      mOuterWindowID = contextOuter->WindowID();
      nsCOMPtr<nsPIDOMWindowOuter> parent = contextOuter->GetScriptableParent();
      mParentOuterWindowID = parent ? parent->WindowID() : mOuterWindowID;
      mTopOuterWindowID = FindTopOuterWindowID(contextOuter);
      RefPtr<dom::BrowsingContext> bc = contextOuter->GetBrowsingContext();
      mBrowsingContextID = bc ? bc->Id() : 0;

      nsGlobalWindowInner* innerWindow =
          nsGlobalWindowInner::Cast(contextOuter->GetCurrentInnerWindow());
      if (innerWindow) {
        mTopLevelPrincipal = innerWindow->GetTopLevelPrincipal();

        // The top-level-storage-area-principal is not null only for the first
        // level of iframes (null for top-level contexts, and null for
        // sub-iframes). If we are loading a sub-document resource, we must
        // calculate what the top-level-storage-area-principal will be for the
        // new context.
        if (externalType != nsIContentPolicy::TYPE_SUBDOCUMENT) {
          mTopLevelStorageAreaPrincipal =
              innerWindow->GetTopLevelStorageAreaPrincipal();
        } else if (contextOuter->IsTopLevelWindow()) {
          Document* doc = innerWindow->GetExtantDoc();
          if (!doc || (!doc->StorageAccessSandboxed() &&
                       !nsContentUtils::IsInPrivateBrowsing(doc))) {
            mTopLevelStorageAreaPrincipal = innerWindow->GetPrincipal();
          }

          // If this is the first level iframe, innerWindow is our top-level
          // principal.
          if (!mTopLevelPrincipal) {
            mTopLevelPrincipal = innerWindow->GetPrincipal();
          }
        }

        mDocumentHasLoaded = innerWindow->IsDocumentLoaded();

        if (innerWindow->IsFrame()) {
          // For resources within iframes, we actually want the
          // top-level document's flag, not the iframe document's.
          mDocumentHasLoaded = false;
          nsGlobalWindowOuter* topOuter =
              innerWindow->GetScriptableTopInternal();
          if (topOuter) {
            nsGlobalWindowInner* topInner =
                nsGlobalWindowInner::Cast(topOuter->GetCurrentInnerWindow());
            if (topInner) {
              mDocumentHasLoaded = topInner->IsDocumentLoaded();
            }
          }
        }
      }

      // Let's inherit the cookie behavior and permission from the parent
      // document.
      mCookieSettings = aLoadingContext->OwnerDoc()->CookieSettings();
    }

    mInnerWindowID = aLoadingContext->OwnerDoc()->InnerWindowID();
    mAncestorPrincipals = aLoadingContext->OwnerDoc()->AncestorPrincipals();
    mAncestorOuterWindowIDs =
        aLoadingContext->OwnerDoc()->AncestorOuterWindowIDs();
    MOZ_DIAGNOSTIC_ASSERT(mAncestorPrincipals.Length() ==
                          mAncestorOuterWindowIDs.Length());
    mDocumentHasUserInteracted =
        aLoadingContext->OwnerDoc()->UserHasInteracted();

    // When the element being loaded is a frame, we choose the frame's window
    // for the window ID and the frame element's window as the parent
    // window. This is the behavior that Chrome exposes to add-ons.
    // NB: If the frameLoaderOwner doesn't have a frame loader, then the load
    // must be coming from an object (such as a plugin) that's loaded into it
    // instead of a document being loaded. In that case, treat this object like
    // any other non-document-loading element.
    RefPtr<nsFrameLoaderOwner> frameLoaderOwner =
        do_QueryObject(aLoadingContext);
    RefPtr<nsFrameLoader> fl =
        frameLoaderOwner ? frameLoaderOwner->GetFrameLoader() : nullptr;
    if (fl) {
      nsCOMPtr<nsIDocShell> docShell = fl->GetDocShell(IgnoreErrors());
      if (docShell) {
        nsCOMPtr<nsPIDOMWindowOuter> outerWindow = do_GetInterface(docShell);
        if (outerWindow) {
          mFrameOuterWindowID = outerWindow->WindowID();

          RefPtr<dom::BrowsingContext> bc = outerWindow->GetBrowsingContext();
          mFrameBrowsingContextID = bc ? bc->Id() : 0;
        }
      }
    }

    // if the document forces all requests to be upgraded from http to https,
    // then we should do that for all requests. If it only forces preloads to be
    // upgraded then we should enforce upgrade insecure requests only for
    // preloads.
    mUpgradeInsecureRequests =
        aLoadingContext->OwnerDoc()->GetUpgradeInsecureRequests(false) ||
        (nsContentUtils::IsPreloadType(mInternalContentPolicyType) &&
         aLoadingContext->OwnerDoc()->GetUpgradeInsecureRequests(true));

    if (nsContentUtils::IsUpgradableDisplayType(externalType)) {
      nsCOMPtr<nsIURI> uri;
      mLoadingPrincipal->GetURI(getter_AddRefs(uri));
      if (uri) {
        // Checking https not secure context as http://localhost can't be
        // upgraded
        bool isHttpsScheme;
        nsresult rv = uri->SchemeIs("https", &isHttpsScheme);
        if (NS_SUCCEEDED(rv) && isHttpsScheme) {
          if (nsMixedContentBlocker::ShouldUpgradeMixedDisplayContent()) {
            mBrowserUpgradeInsecureRequests = true;
          } else {
            mBrowserWouldUpgradeInsecureRequests = true;
          }
        }
      }
    }
  }

  mOriginAttributes = mLoadingPrincipal->OriginAttributesRef();

  // We need to do this after inheriting the document's origin attributes
  // above, in case the loading principal ends up being the system principal.
  if (aLoadingContext) {
    nsCOMPtr<nsILoadContext> loadContext =
        aLoadingContext->OwnerDoc()->GetLoadContext();
    nsCOMPtr<nsIDocShell> docShell = aLoadingContext->OwnerDoc()->GetDocShell();
    if (loadContext && docShell &&
        docShell->ItemType() == nsIDocShellTreeItem::typeContent) {
      bool usePrivateBrowsing;
      nsresult rv = loadContext->GetUsePrivateBrowsing(&usePrivateBrowsing);
      if (NS_SUCCEEDED(rv)) {
        mOriginAttributes.SyncAttributesWithPrivateBrowsing(usePrivateBrowsing);
      }
    }
  }

  // For chrome docshell, the mPrivateBrowsingId remains 0 even its
  // UsePrivateBrowsing() is true, so we only update the mPrivateBrowsingId in
  // origin attributes if the type of the docshell is content.
  if (aLoadingContext) {
    nsCOMPtr<nsIDocShell> docShell = aLoadingContext->OwnerDoc()->GetDocShell();
    if (docShell) {
      if (docShell->ItemType() == nsIDocShellTreeItem::typeChrome) {
        MOZ_ASSERT(mOriginAttributes.mPrivateBrowsingId == 0,
                   "chrome docshell shouldn't have mPrivateBrowsingId set.");
      }
    }
  }
}

/* Constructor takes an outer window, but no loadingNode or loadingPrincipal.
 * This constructor should only be used for TYPE_DOCUMENT loads, since they
 * have a null loadingNode and loadingPrincipal.
 */
LoadInfo::LoadInfo(nsPIDOMWindowOuter* aOuterWindow,
                   nsIPrincipal* aTriggeringPrincipal,
                   nsISupports* aContextForTopLevelLoad,
                   nsSecurityFlags aSecurityFlags)
    : mLoadingPrincipal(nullptr),
      mTriggeringPrincipal(aTriggeringPrincipal),
      mPrincipalToInherit(nullptr),
      mContextForTopLevelLoad(do_GetWeakReference(aContextForTopLevelLoad)),
      mSecurityFlags(aSecurityFlags),
      mInternalContentPolicyType(nsIContentPolicy::TYPE_DOCUMENT),
      mTainting(LoadTainting::Basic),
      mUpgradeInsecureRequests(false),
      mBrowserUpgradeInsecureRequests(false),
      mBrowserWouldUpgradeInsecureRequests(false),
      mForceAllowDataURI(false),
      mAllowInsecureRedirectToDataURI(false),
      mSkipContentPolicyCheckForWebRequest(false),
      mOriginalFrameSrcLoad(false),
      mForceInheritPrincipalDropped(false),
      mInnerWindowID(0),
      mOuterWindowID(0),
      mParentOuterWindowID(0),
      mTopOuterWindowID(0),
      mFrameOuterWindowID(0),
      mBrowsingContextID(0),
      mFrameBrowsingContextID(0),
      mInitialSecurityCheckDone(false),
      mIsThirdPartyContext(false),  // NB: TYPE_DOCUMENT implies !third-party.
      mIsDocshellReload(false),
      mSendCSPViolationEvents(true),
      mRequestBlockingReason(BLOCKING_REASON_NONE),
      mForcePreflight(false),
      mIsPreflight(false),
      mLoadTriggeredFromExternal(false),
      mServiceWorkerTaintingSynthesized(false),
      mDocumentHasUserInteracted(false),
      mDocumentHasLoaded(false),
      mIsFromProcessingFrameAttributes(false) {
  // Top-level loads are never third-party
  // Grab the information we can out of the window.
  MOZ_ASSERT(aOuterWindow);
  MOZ_ASSERT(mTriggeringPrincipal);

  // if the load is sandboxed, we can not also inherit the principal
  if (mSecurityFlags & nsILoadInfo::SEC_SANDBOXED) {
    mForceInheritPrincipalDropped =
        (mSecurityFlags & nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL);
    mSecurityFlags &= ~nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL;
  }

  // NB: Ignore the current inner window since we're navigating away from it.
  mOuterWindowID = aOuterWindow->WindowID();
  RefPtr<BrowsingContext> bc = aOuterWindow->GetBrowsingContext();
  mBrowsingContextID = bc ? bc->Id() : 0;

  // TODO We can have a parent without a frame element in some cases dealing
  // with the hidden window.
  nsCOMPtr<nsPIDOMWindowOuter> parent = aOuterWindow->GetScriptableParent();
  mParentOuterWindowID = parent ? parent->WindowID() : 0;
  mTopOuterWindowID = FindTopOuterWindowID(aOuterWindow);

  nsGlobalWindowInner* innerWindow =
      nsGlobalWindowInner::Cast(aOuterWindow->GetCurrentInnerWindow());
  if (innerWindow) {
    mTopLevelPrincipal = innerWindow->GetTopLevelPrincipal();
    // mTopLevelStorageAreaPrincipal is always null for top-level document
    // loading.
  }

  // get the docshell from the outerwindow, and then get the originattributes
  nsCOMPtr<nsIDocShell> docShell = aOuterWindow->GetDocShell();
  MOZ_ASSERT(docShell);
  mOriginAttributes = nsDocShell::Cast(docShell)->GetOriginAttributes();
  mAncestorPrincipals = nsDocShell::Cast(docShell)->AncestorPrincipals();
  mAncestorOuterWindowIDs =
      nsDocShell::Cast(docShell)->AncestorOuterWindowIDs();
  MOZ_DIAGNOSTIC_ASSERT(mAncestorPrincipals.Length() ==
                        mAncestorOuterWindowIDs.Length());

#ifdef DEBUG
  if (docShell->ItemType() == nsIDocShellTreeItem::typeChrome) {
    MOZ_ASSERT(mOriginAttributes.mPrivateBrowsingId == 0,
               "chrome docshell shouldn't have mPrivateBrowsingId set.");
  }
#endif

  // Let's take the current cookie behavior and current cookie permission
  // for the documents' loadInfo. Note that for any other loadInfos,
  // cookieBehavior will be BEHAVIOR_REJECT for security reasons.
  mCookieSettings = CookieSettings::Create();
}

LoadInfo::LoadInfo(const LoadInfo& rhs)
    : mLoadingPrincipal(rhs.mLoadingPrincipal),
      mTriggeringPrincipal(rhs.mTriggeringPrincipal),
      mPrincipalToInherit(rhs.mPrincipalToInherit),
      mSandboxedLoadingPrincipal(rhs.mSandboxedLoadingPrincipal),
      mTopLevelPrincipal(rhs.mTopLevelPrincipal),
      mTopLevelStorageAreaPrincipal(rhs.mTopLevelStorageAreaPrincipal),
      mResultPrincipalURI(rhs.mResultPrincipalURI),
      mCookieSettings(rhs.mCookieSettings),
      mClientInfo(rhs.mClientInfo),
      // mReservedClientSource must be handled specially during redirect
      // mReservedClientInfo must be handled specially during redirect
      // mInitialClientInfo must be handled specially during redirect
      mController(rhs.mController),
      mPerformanceStorage(rhs.mPerformanceStorage),
      mLoadingContext(rhs.mLoadingContext),
      mContextForTopLevelLoad(rhs.mContextForTopLevelLoad),
      mSecurityFlags(rhs.mSecurityFlags),
      mInternalContentPolicyType(rhs.mInternalContentPolicyType),
      mTainting(rhs.mTainting),
      mUpgradeInsecureRequests(rhs.mUpgradeInsecureRequests),
      mBrowserUpgradeInsecureRequests(rhs.mBrowserUpgradeInsecureRequests),
      mBrowserWouldUpgradeInsecureRequests(
          rhs.mBrowserWouldUpgradeInsecureRequests),
      mForceAllowDataURI(rhs.mForceAllowDataURI),
      mAllowInsecureRedirectToDataURI(rhs.mAllowInsecureRedirectToDataURI),
      mSkipContentPolicyCheckForWebRequest(
          rhs.mSkipContentPolicyCheckForWebRequest),
      mOriginalFrameSrcLoad(rhs.mOriginalFrameSrcLoad),
      mForceInheritPrincipalDropped(rhs.mForceInheritPrincipalDropped),
      mInnerWindowID(rhs.mInnerWindowID),
      mOuterWindowID(rhs.mOuterWindowID),
      mParentOuterWindowID(rhs.mParentOuterWindowID),
      mTopOuterWindowID(rhs.mTopOuterWindowID),
      mFrameOuterWindowID(rhs.mFrameOuterWindowID),
      mBrowsingContextID(rhs.mBrowsingContextID),
      mFrameBrowsingContextID(rhs.mFrameBrowsingContextID),
      mInitialSecurityCheckDone(rhs.mInitialSecurityCheckDone),
      mIsThirdPartyContext(rhs.mIsThirdPartyContext),
      mIsDocshellReload(rhs.mIsDocshellReload),
      mSendCSPViolationEvents(rhs.mSendCSPViolationEvents),
      mOriginAttributes(rhs.mOriginAttributes),
      mRedirectChainIncludingInternalRedirects(
          rhs.mRedirectChainIncludingInternalRedirects),
      mRedirectChain(rhs.mRedirectChain),
      mAncestorPrincipals(rhs.mAncestorPrincipals),
      mAncestorOuterWindowIDs(rhs.mAncestorOuterWindowIDs),
      mCorsUnsafeHeaders(rhs.mCorsUnsafeHeaders),
      mRequestBlockingReason(rhs.mRequestBlockingReason),
      mForcePreflight(rhs.mForcePreflight),
      mIsPreflight(rhs.mIsPreflight),
      mLoadTriggeredFromExternal(rhs.mLoadTriggeredFromExternal),
      // mServiceWorkerTaintingSynthesized must be handled specially during
      // redirect
      mServiceWorkerTaintingSynthesized(false),
      mDocumentHasUserInteracted(rhs.mDocumentHasUserInteracted),
      mDocumentHasLoaded(rhs.mDocumentHasLoaded),
      mCspNonce(rhs.mCspNonce),
      mIsFromProcessingFrameAttributes(rhs.mIsFromProcessingFrameAttributes) {}

LoadInfo::LoadInfo(
    nsIPrincipal* aLoadingPrincipal, nsIPrincipal* aTriggeringPrincipal,
    nsIPrincipal* aPrincipalToInherit, nsIPrincipal* aSandboxedLoadingPrincipal,
    nsIPrincipal* aTopLevelPrincipal,
    nsIPrincipal* aTopLevelStorageAreaPrincipal, nsIURI* aResultPrincipalURI,
    nsICookieSettings* aCookieSettings, const Maybe<ClientInfo>& aClientInfo,
    const Maybe<ClientInfo>& aReservedClientInfo,
    const Maybe<ClientInfo>& aInitialClientInfo,
    const Maybe<ServiceWorkerDescriptor>& aController,
    nsSecurityFlags aSecurityFlags, nsContentPolicyType aContentPolicyType,
    LoadTainting aTainting, bool aUpgradeInsecureRequests,
    bool aBrowserUpgradeInsecureRequests,
    bool aBrowserWouldUpgradeInsecureRequests, bool aForceAllowDataURI,
    bool aAllowInsecureRedirectToDataURI,
    bool aSkipContentPolicyCheckForWebRequest,
    bool aForceInheritPrincipalDropped, uint64_t aInnerWindowID,
    uint64_t aOuterWindowID, uint64_t aParentOuterWindowID,
    uint64_t aTopOuterWindowID, uint64_t aFrameOuterWindowID,
    uint64_t aBrowsingContextID, uint64_t aFrameBrowsingContextID,
    bool aInitialSecurityCheckDone, bool aIsThirdPartyContext,
    bool aIsDocshellReload, bool aSendCSPViolationEvents,
    const OriginAttributes& aOriginAttributes,
    RedirectHistoryArray& aRedirectChainIncludingInternalRedirects,
    RedirectHistoryArray& aRedirectChain,
    nsTArray<nsCOMPtr<nsIPrincipal>>&& aAncestorPrincipals,
    const nsTArray<uint64_t>& aAncestorOuterWindowIDs,
    const nsTArray<nsCString>& aCorsUnsafeHeaders, bool aForcePreflight,
    bool aIsPreflight, bool aLoadTriggeredFromExternal,
    bool aServiceWorkerTaintingSynthesized, bool aDocumentHasUserInteracted,
    bool aDocumentHasLoaded, const nsAString& aCspNonce,
    uint32_t aRequestBlockingReason)
    : mLoadingPrincipal(aLoadingPrincipal),
      mTriggeringPrincipal(aTriggeringPrincipal),
      mPrincipalToInherit(aPrincipalToInherit),
      mTopLevelPrincipal(aTopLevelPrincipal),
      mTopLevelStorageAreaPrincipal(aTopLevelStorageAreaPrincipal),
      mResultPrincipalURI(aResultPrincipalURI),
      mCookieSettings(aCookieSettings),
      mClientInfo(aClientInfo),
      mReservedClientInfo(aReservedClientInfo),
      mInitialClientInfo(aInitialClientInfo),
      mController(aController),
      mSecurityFlags(aSecurityFlags),
      mInternalContentPolicyType(aContentPolicyType),
      mTainting(aTainting),
      mUpgradeInsecureRequests(aUpgradeInsecureRequests),
      mBrowserUpgradeInsecureRequests(aBrowserUpgradeInsecureRequests),
      mBrowserWouldUpgradeInsecureRequests(
          aBrowserWouldUpgradeInsecureRequests),
      mForceAllowDataURI(aForceAllowDataURI),
      mAllowInsecureRedirectToDataURI(aAllowInsecureRedirectToDataURI),
      mSkipContentPolicyCheckForWebRequest(
          aSkipContentPolicyCheckForWebRequest),
      mOriginalFrameSrcLoad(false),
      mForceInheritPrincipalDropped(aForceInheritPrincipalDropped),
      mInnerWindowID(aInnerWindowID),
      mOuterWindowID(aOuterWindowID),
      mParentOuterWindowID(aParentOuterWindowID),
      mTopOuterWindowID(aTopOuterWindowID),
      mFrameOuterWindowID(aFrameOuterWindowID),
      mBrowsingContextID(aBrowsingContextID),
      mFrameBrowsingContextID(aFrameBrowsingContextID),
      mInitialSecurityCheckDone(aInitialSecurityCheckDone),
      mIsThirdPartyContext(aIsThirdPartyContext),
      mIsDocshellReload(aIsDocshellReload),
      mSendCSPViolationEvents(aSendCSPViolationEvents),
      mOriginAttributes(aOriginAttributes),
      mAncestorPrincipals(std::move(aAncestorPrincipals)),
      mAncestorOuterWindowIDs(aAncestorOuterWindowIDs),
      mCorsUnsafeHeaders(aCorsUnsafeHeaders),
      mRequestBlockingReason(aRequestBlockingReason),
      mForcePreflight(aForcePreflight),
      mIsPreflight(aIsPreflight),
      mLoadTriggeredFromExternal(aLoadTriggeredFromExternal),
      mServiceWorkerTaintingSynthesized(aServiceWorkerTaintingSynthesized),
      mDocumentHasUserInteracted(aDocumentHasUserInteracted),
      mDocumentHasLoaded(aDocumentHasLoaded),
      mCspNonce(aCspNonce),
      mIsFromProcessingFrameAttributes(false) {
  // Only top level TYPE_DOCUMENT loads can have a null loadingPrincipal
  MOZ_ASSERT(mLoadingPrincipal ||
             aContentPolicyType == nsIContentPolicy::TYPE_DOCUMENT);
  MOZ_ASSERT(mTriggeringPrincipal);

  mRedirectChainIncludingInternalRedirects.SwapElements(
      aRedirectChainIncludingInternalRedirects);

  mRedirectChain.SwapElements(aRedirectChain);
}

void LoadInfo::ComputeIsThirdPartyContext(nsPIDOMWindowOuter* aOuterWindow) {
  nsContentPolicyType type =
      nsContentUtils::InternalContentPolicyTypeToExternal(
          mInternalContentPolicyType);
  if (type == nsIContentPolicy::TYPE_DOCUMENT) {
    // Top-level loads are never third-party.
    mIsThirdPartyContext = false;
    return;
  }

  nsCOMPtr<mozIThirdPartyUtil> util(do_GetService(THIRDPARTYUTIL_CONTRACTID));
  if (NS_WARN_IF(!util)) {
    return;
  }

  util->IsThirdPartyWindow(aOuterWindow, nullptr, &mIsThirdPartyContext);
}

NS_IMPL_ISUPPORTS(LoadInfo, nsILoadInfo)

already_AddRefed<nsILoadInfo> LoadInfo::Clone() const {
  RefPtr<LoadInfo> copy(new LoadInfo(*this));
  return copy.forget();
}

already_AddRefed<nsILoadInfo> LoadInfo::CloneWithNewSecFlags(
    nsSecurityFlags aSecurityFlags) const {
  RefPtr<LoadInfo> copy(new LoadInfo(*this));
  copy->mSecurityFlags = aSecurityFlags;
  return copy.forget();
}

already_AddRefed<nsILoadInfo> LoadInfo::CloneForNewRequest() const {
  RefPtr<LoadInfo> copy(new LoadInfo(*this));
  copy->mInitialSecurityCheckDone = false;
  copy->mRedirectChainIncludingInternalRedirects.Clear();
  copy->mRedirectChain.Clear();
  copy->mResultPrincipalURI = nullptr;
  return copy.forget();
}

NS_IMETHODIMP
LoadInfo::GetLoadingPrincipal(nsIPrincipal** aLoadingPrincipal) {
  NS_IF_ADDREF(*aLoadingPrincipal = mLoadingPrincipal);
  return NS_OK;
}

nsIPrincipal* LoadInfo::LoadingPrincipal() { return mLoadingPrincipal; }

NS_IMETHODIMP
LoadInfo::GetTriggeringPrincipal(nsIPrincipal** aTriggeringPrincipal) {
  NS_ADDREF(*aTriggeringPrincipal = mTriggeringPrincipal);
  return NS_OK;
}

nsIPrincipal* LoadInfo::TriggeringPrincipal() { return mTriggeringPrincipal; }

NS_IMETHODIMP
LoadInfo::GetPrincipalToInherit(nsIPrincipal** aPrincipalToInherit) {
  NS_IF_ADDREF(*aPrincipalToInherit = mPrincipalToInherit);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetPrincipalToInherit(nsIPrincipal* aPrincipalToInherit) {
  MOZ_ASSERT(aPrincipalToInherit, "must be a valid principal to inherit");
  mPrincipalToInherit = aPrincipalToInherit;
  return NS_OK;
}

nsIPrincipal* LoadInfo::PrincipalToInherit() { return mPrincipalToInherit; }

nsIPrincipal* LoadInfo::FindPrincipalToInherit(nsIChannel* aChannel) {
  if (mPrincipalToInherit) {
    return mPrincipalToInherit;
  }

  nsCOMPtr<nsIURI> uri = mResultPrincipalURI;
  if (!uri) {
    Unused << aChannel->GetOriginalURI(getter_AddRefs(uri));
  }

  auto prin = BasePrincipal::Cast(mTriggeringPrincipal);
  return prin->PrincipalToInherit(uri);
}

nsIPrincipal* LoadInfo::GetSandboxedLoadingPrincipal() {
  if (!(mSecurityFlags & nsILoadInfo::SEC_SANDBOXED)) {
    return nullptr;
  }

  if (!mSandboxedLoadingPrincipal) {
    if (mLoadingPrincipal) {
      mSandboxedLoadingPrincipal =
          NullPrincipal::CreateWithInheritedAttributes(mLoadingPrincipal);
    } else {
      OriginAttributes attrs(mOriginAttributes);
      mSandboxedLoadingPrincipal = NullPrincipal::Create(attrs);
    }
  }
  MOZ_ASSERT(mSandboxedLoadingPrincipal);

  return mSandboxedLoadingPrincipal;
}

nsIPrincipal* LoadInfo::GetTopLevelPrincipal() { return mTopLevelPrincipal; }

nsIPrincipal* LoadInfo::GetTopLevelStorageAreaPrincipal() {
  return mTopLevelStorageAreaPrincipal;
}

NS_IMETHODIMP
LoadInfo::GetLoadingDocument(Document** aResult) {
  if (nsCOMPtr<nsINode> node = do_QueryReferent(mLoadingContext)) {
    RefPtr<Document> context = node->OwnerDoc();
    context.forget(aResult);
  }
  return NS_OK;
}

nsINode* LoadInfo::LoadingNode() {
  nsCOMPtr<nsINode> node = do_QueryReferent(mLoadingContext);
  return node;
}

already_AddRefed<nsISupports> LoadInfo::ContextForTopLevelLoad() {
  // Most likely you want to query LoadingNode() instead of
  // ContextForTopLevelLoad() if this assertion fires.
  MOZ_ASSERT(mInternalContentPolicyType == nsIContentPolicy::TYPE_DOCUMENT,
             "should only query this context for top level document loads");
  nsCOMPtr<nsISupports> context = do_QueryReferent(mContextForTopLevelLoad);
  return context.forget();
}

already_AddRefed<nsISupports> LoadInfo::GetLoadingContext() {
  nsCOMPtr<nsISupports> context;
  if (mInternalContentPolicyType == nsIContentPolicy::TYPE_DOCUMENT) {
    context = ContextForTopLevelLoad();
  } else {
    context = LoadingNode();
  }
  return context.forget();
}

NS_IMETHODIMP
LoadInfo::GetLoadingContextXPCOM(nsISupports** aResult) {
  nsCOMPtr<nsISupports> context = GetLoadingContext();
  context.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetSecurityFlags(nsSecurityFlags* aResult) {
  *aResult = mSecurityFlags;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetSecurityMode(uint32_t* aFlags) {
  *aFlags =
      (mSecurityFlags & (nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS |
                         nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED |
                         nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS |
                         nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL |
                         nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS));
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetIsInThirdPartyContext(bool* aIsInThirdPartyContext) {
  *aIsInThirdPartyContext = mIsThirdPartyContext;
  return NS_OK;
}

static const uint32_t sCookiePolicyMask =
    nsILoadInfo::SEC_COOKIES_DEFAULT | nsILoadInfo::SEC_COOKIES_INCLUDE |
    nsILoadInfo::SEC_COOKIES_SAME_ORIGIN | nsILoadInfo::SEC_COOKIES_OMIT;

NS_IMETHODIMP
LoadInfo::GetCookiePolicy(uint32_t* aResult) {
  uint32_t policy = mSecurityFlags & sCookiePolicyMask;
  if (policy == nsILoadInfo::SEC_COOKIES_DEFAULT) {
    policy = (mSecurityFlags & SEC_REQUIRE_CORS_DATA_INHERITS)
                 ? nsILoadInfo::SEC_COOKIES_SAME_ORIGIN
                 : nsILoadInfo::SEC_COOKIES_INCLUDE;
  }

  *aResult = policy;
  return NS_OK;
}

namespace {

already_AddRefed<nsICookieSettings> CreateCookieSettings(
    nsContentPolicyType aContentPolicyType) {
  if (StaticPrefs::network_cookieSettings_unblocked_for_testing()) {
    return CookieSettings::Create();
  }

  // These contentPolictTypes require a real CookieSettings because favicon and
  // save-as requests must send cookies. Anything else should not send/receive
  // cookies.
  if (aContentPolicyType == nsIContentPolicy::TYPE_INTERNAL_IMAGE_FAVICON ||
      aContentPolicyType == nsIContentPolicy::TYPE_SAVEAS_DOWNLOAD) {
    return CookieSettings::Create();
  }

  return CookieSettings::CreateBlockingAll();
}

}  // namespace

NS_IMETHODIMP
LoadInfo::GetCookieSettings(nsICookieSettings** aCookieSettings) {
  if (!mCookieSettings) {
    mCookieSettings = CreateCookieSettings(mInternalContentPolicyType);
  }

  nsCOMPtr<nsICookieSettings> cookieSettings = mCookieSettings;
  cookieSettings.forget(aCookieSettings);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetCookieSettings(nsICookieSettings* aCookieSettings) {
  MOZ_ASSERT(aCookieSettings);
  // We allow the overwrite of CookieSettings.
  mCookieSettings = aCookieSettings;
  return NS_OK;
}

void LoadInfo::SetIncludeCookiesSecFlag() {
  MOZ_ASSERT((mSecurityFlags & sCookiePolicyMask) ==
             nsILoadInfo::SEC_COOKIES_DEFAULT);
  mSecurityFlags =
      (mSecurityFlags & ~sCookiePolicyMask) | nsILoadInfo::SEC_COOKIES_INCLUDE;
}

NS_IMETHODIMP
LoadInfo::GetForceInheritPrincipal(bool* aInheritPrincipal) {
  *aInheritPrincipal =
      (mSecurityFlags & nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetForceInheritPrincipalOverruleOwner(bool* aInheritPrincipal) {
  *aInheritPrincipal =
      (mSecurityFlags &
       nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL_OVERRULE_OWNER);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetLoadingSandboxed(bool* aLoadingSandboxed) {
  *aLoadingSandboxed = (mSecurityFlags & nsILoadInfo::SEC_SANDBOXED);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetAboutBlankInherits(bool* aResult) {
  *aResult = (mSecurityFlags & nsILoadInfo::SEC_ABOUT_BLANK_INHERITS);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetAllowChrome(bool* aResult) {
  *aResult = (mSecurityFlags & nsILoadInfo::SEC_ALLOW_CHROME);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetDisallowScript(bool* aResult) {
  *aResult = (mSecurityFlags & nsILoadInfo::SEC_DISALLOW_SCRIPT);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetDontFollowRedirects(bool* aResult) {
  *aResult = (mSecurityFlags & nsILoadInfo::SEC_DONT_FOLLOW_REDIRECTS);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetLoadErrorPage(bool* aResult) {
  *aResult = (mSecurityFlags & nsILoadInfo::SEC_LOAD_ERROR_PAGE);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetIsDocshellReload(bool* aResult) {
  *aResult = mIsDocshellReload;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetIsDocshellReload(bool aValue) {
  mIsDocshellReload = aValue;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetSendCSPViolationEvents(bool* aResult) {
  *aResult = mSendCSPViolationEvents;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetSendCSPViolationEvents(bool aValue) {
  mSendCSPViolationEvents = aValue;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetExternalContentPolicyType(nsContentPolicyType* aResult) {
  *aResult = nsContentUtils::InternalContentPolicyTypeToExternal(
      mInternalContentPolicyType);
  return NS_OK;
}

nsContentPolicyType LoadInfo::InternalContentPolicyType() {
  return mInternalContentPolicyType;
}

NS_IMETHODIMP
LoadInfo::GetUpgradeInsecureRequests(bool* aResult) {
  *aResult = mUpgradeInsecureRequests;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetBrowserUpgradeInsecureRequests(bool* aResult) {
  *aResult = mBrowserUpgradeInsecureRequests;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetBrowserWouldUpgradeInsecureRequests(bool* aResult) {
  *aResult = mBrowserWouldUpgradeInsecureRequests;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetForceAllowDataURI(bool aForceAllowDataURI) {
  MOZ_ASSERT(!mForceAllowDataURI ||
                 mInternalContentPolicyType == nsIContentPolicy::TYPE_DOCUMENT,
             "can only allow data URI navigation for TYPE_DOCUMENT");
  mForceAllowDataURI = aForceAllowDataURI;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetForceAllowDataURI(bool* aForceAllowDataURI) {
  *aForceAllowDataURI = mForceAllowDataURI;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetAllowInsecureRedirectToDataURI(
    bool aAllowInsecureRedirectToDataURI) {
  mAllowInsecureRedirectToDataURI = aAllowInsecureRedirectToDataURI;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetAllowInsecureRedirectToDataURI(
    bool* aAllowInsecureRedirectToDataURI) {
  *aAllowInsecureRedirectToDataURI = mAllowInsecureRedirectToDataURI;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetSkipContentPolicyCheckForWebRequest(bool aSkip) {
  mSkipContentPolicyCheckForWebRequest = aSkip;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetSkipContentPolicyCheckForWebRequest(bool* aSkip) {
  *aSkip = mSkipContentPolicyCheckForWebRequest;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetOriginalFrameSrcLoad(bool aOriginalFrameSrcLoad) {
  mOriginalFrameSrcLoad = aOriginalFrameSrcLoad;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetOriginalFrameSrcLoad(bool* aOriginalFrameSrcLoad) {
  *aOriginalFrameSrcLoad = mOriginalFrameSrcLoad;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetForceInheritPrincipalDropped(bool* aResult) {
  *aResult = mForceInheritPrincipalDropped;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetInnerWindowID(uint64_t* aResult) {
  *aResult = mInnerWindowID;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetOuterWindowID(uint64_t* aResult) {
  *aResult = mOuterWindowID;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetParentOuterWindowID(uint64_t* aResult) {
  *aResult = mParentOuterWindowID;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetTopOuterWindowID(uint64_t* aResult) {
  *aResult = mTopOuterWindowID;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetFrameOuterWindowID(uint64_t* aResult) {
  *aResult = mFrameOuterWindowID;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetBrowsingContextID(uint64_t* aResult) {
  *aResult = mBrowsingContextID;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetFrameBrowsingContextID(uint64_t* aResult) {
  *aResult = mFrameBrowsingContextID;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetBrowsingContext(dom::BrowsingContext** aResult) {
  *aResult = BrowsingContext::Get(mBrowsingContextID).take();
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetFrameBrowsingContext(dom::BrowsingContext** aResult) {
  *aResult = BrowsingContext::Get(mFrameBrowsingContextID).take();
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetScriptableOriginAttributes(
    JSContext* aCx, JS::MutableHandle<JS::Value> aOriginAttributes) {
  if (NS_WARN_IF(!ToJSValue(aCx, mOriginAttributes, aOriginAttributes))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::ResetPrincipalToInheritToNullPrincipal() {
  // take the originAttributes from the LoadInfo and create
  // a new NullPrincipal using those origin attributes.
  nsCOMPtr<nsIPrincipal> newNullPrincipal =
      NullPrincipal::Create(mOriginAttributes);

  mPrincipalToInherit = newNullPrincipal;

  // setting SEC_FORCE_INHERIT_PRINCIPAL_OVERRULE_OWNER will overrule
  // any non null owner set on the channel and will return the principal
  // form the loadinfo instead.
  mSecurityFlags |= SEC_FORCE_INHERIT_PRINCIPAL_OVERRULE_OWNER;

  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetScriptableOriginAttributes(
    JSContext* aCx, JS::Handle<JS::Value> aOriginAttributes) {
  OriginAttributes attrs;
  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  mOriginAttributes = attrs;
  return NS_OK;
}

nsresult LoadInfo::GetOriginAttributes(
    mozilla::OriginAttributes* aOriginAttributes) {
  NS_ENSURE_ARG(aOriginAttributes);
  *aOriginAttributes = mOriginAttributes;
  return NS_OK;
}

nsresult LoadInfo::SetOriginAttributes(
    const mozilla::OriginAttributes& aOriginAttributes) {
  mOriginAttributes = aOriginAttributes;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetInitialSecurityCheckDone(bool aInitialSecurityCheckDone) {
  // Indicates whether the channel was ever evaluated by the
  // ContentSecurityManager. Once set to true, this flag must
  // remain true throughout the lifetime of the channel.
  // Setting it to anything else than true will be discarded.
  MOZ_ASSERT(aInitialSecurityCheckDone,
             "aInitialSecurityCheckDone must be true");
  mInitialSecurityCheckDone =
      mInitialSecurityCheckDone || aInitialSecurityCheckDone;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetInitialSecurityCheckDone(bool* aResult) {
  *aResult = mInitialSecurityCheckDone;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::AppendRedirectHistoryEntry(nsIRedirectHistoryEntry* aEntry,
                                     bool aIsInternalRedirect) {
  NS_ENSURE_ARG(aEntry);
  MOZ_ASSERT(NS_IsMainThread());

  mRedirectChainIncludingInternalRedirects.AppendElement(aEntry);
  if (!aIsInternalRedirect) {
    mRedirectChain.AppendElement(aEntry);
  }
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetRedirects(JSContext* aCx, JS::MutableHandle<JS::Value> aRedirects,
                       const RedirectHistoryArray& aArray) {
  JS::Rooted<JSObject*> redirects(aCx, JS_NewArrayObject(aCx, aArray.Length()));
  NS_ENSURE_TRUE(redirects, NS_ERROR_OUT_OF_MEMORY);

  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
  NS_ENSURE_TRUE(global, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIXPConnect> xpc = nsIXPConnect::XPConnect();

  for (size_t idx = 0; idx < aArray.Length(); idx++) {
    JS::RootedObject jsobj(aCx);
    nsresult rv =
        xpc->WrapNative(aCx, global, aArray[idx],
                        NS_GET_IID(nsIRedirectHistoryEntry), jsobj.address());
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_STATE(jsobj);

    bool rc = JS_DefineElement(aCx, redirects, idx, jsobj, JSPROP_ENUMERATE);
    NS_ENSURE_TRUE(rc, NS_ERROR_UNEXPECTED);
  }

  aRedirects.setObject(*redirects);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetRedirectChainIncludingInternalRedirects(
    JSContext* aCx, JS::MutableHandle<JS::Value> aChain) {
  return GetRedirects(aCx, aChain, mRedirectChainIncludingInternalRedirects);
}

const RedirectHistoryArray&
LoadInfo::RedirectChainIncludingInternalRedirects() {
  return mRedirectChainIncludingInternalRedirects;
}

NS_IMETHODIMP
LoadInfo::GetRedirectChain(JSContext* aCx,
                           JS::MutableHandle<JS::Value> aChain) {
  return GetRedirects(aCx, aChain, mRedirectChain);
}

const RedirectHistoryArray& LoadInfo::RedirectChain() { return mRedirectChain; }

const nsTArray<nsCOMPtr<nsIPrincipal>>& LoadInfo::AncestorPrincipals() {
  return mAncestorPrincipals;
}

const nsTArray<uint64_t>& LoadInfo::AncestorOuterWindowIDs() {
  return mAncestorOuterWindowIDs;
}

void LoadInfo::SetCorsPreflightInfo(const nsTArray<nsCString>& aHeaders,
                                    bool aForcePreflight) {
  MOZ_ASSERT(GetSecurityMode() == nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS);
  MOZ_ASSERT(!mInitialSecurityCheckDone);
  mCorsUnsafeHeaders = aHeaders;
  mForcePreflight = aForcePreflight;
}

const nsTArray<nsCString>& LoadInfo::CorsUnsafeHeaders() {
  return mCorsUnsafeHeaders;
}

NS_IMETHODIMP
LoadInfo::GetForcePreflight(bool* aForcePreflight) {
  *aForcePreflight = mForcePreflight;
  return NS_OK;
}

void LoadInfo::SetIsPreflight() {
  MOZ_ASSERT(GetSecurityMode() == nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS);
  MOZ_ASSERT(!mInitialSecurityCheckDone);
  mIsPreflight = true;
}

void LoadInfo::SetUpgradeInsecureRequests() { mUpgradeInsecureRequests = true; }

void LoadInfo::SetBrowserUpgradeInsecureRequests() {
  mBrowserUpgradeInsecureRequests = true;
}

void LoadInfo::SetBrowserWouldUpgradeInsecureRequests() {
  mBrowserWouldUpgradeInsecureRequests = true;
}

NS_IMETHODIMP
LoadInfo::GetIsPreflight(bool* aIsPreflight) {
  *aIsPreflight = mIsPreflight;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetLoadTriggeredFromExternal(bool aLoadTriggeredFromExternal) {
  MOZ_ASSERT(!aLoadTriggeredFromExternal ||
                 mInternalContentPolicyType == nsIContentPolicy::TYPE_DOCUMENT,
             "can only set load triggered from external for TYPE_DOCUMENT");
  mLoadTriggeredFromExternal = aLoadTriggeredFromExternal;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetLoadTriggeredFromExternal(bool* aLoadTriggeredFromExternal) {
  *aLoadTriggeredFromExternal = mLoadTriggeredFromExternal;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetServiceWorkerTaintingSynthesized(
    bool* aServiceWorkerTaintingSynthesized) {
  MOZ_ASSERT(aServiceWorkerTaintingSynthesized);
  *aServiceWorkerTaintingSynthesized = mServiceWorkerTaintingSynthesized;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetTainting(uint32_t* aTaintingOut) {
  MOZ_ASSERT(aTaintingOut);
  *aTaintingOut = static_cast<uint32_t>(mTainting);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::MaybeIncreaseTainting(uint32_t aTainting) {
  NS_ENSURE_ARG(aTainting <= TAINTING_OPAQUE);

  // Skip if the tainting has been set by the service worker.
  if (mServiceWorkerTaintingSynthesized) {
    return NS_OK;
  }

  LoadTainting tainting = static_cast<LoadTainting>(aTainting);
  if (tainting > mTainting) {
    mTainting = tainting;
  }
  return NS_OK;
}

void LoadInfo::SynthesizeServiceWorkerTainting(LoadTainting aTainting) {
  MOZ_DIAGNOSTIC_ASSERT(aTainting <= LoadTainting::Opaque);
  mTainting = aTainting;

  // Flag to prevent the tainting from being increased.
  mServiceWorkerTaintingSynthesized = true;
}

NS_IMETHODIMP
LoadInfo::GetDocumentHasUserInteracted(bool* aDocumentHasUserInteracted) {
  MOZ_ASSERT(aDocumentHasUserInteracted);
  *aDocumentHasUserInteracted = mDocumentHasUserInteracted;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetDocumentHasUserInteracted(bool aDocumentHasUserInteracted) {
  mDocumentHasUserInteracted = aDocumentHasUserInteracted;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetDocumentHasLoaded(bool* aDocumentHasLoaded) {
  MOZ_ASSERT(aDocumentHasLoaded);
  *aDocumentHasLoaded = mDocumentHasLoaded;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetDocumentHasLoaded(bool aDocumentHasLoaded) {
  mDocumentHasLoaded = aDocumentHasLoaded;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetCspNonce(nsAString& aCspNonce) {
  aCspNonce = mCspNonce;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetCspNonce(const nsAString& aCspNonce) {
  MOZ_ASSERT(!mInitialSecurityCheckDone,
             "setting the nonce is only allowed before any sec checks");
  mCspNonce = aCspNonce;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetIsTopLevelLoad(bool* aResult) {
  *aResult = mFrameOuterWindowID ? mFrameOuterWindowID == mOuterWindowID
                                 : mParentOuterWindowID == mOuterWindowID;
  return NS_OK;
}

void LoadInfo::SetIsFromProcessingFrameAttributes() {
  mIsFromProcessingFrameAttributes = true;
}

NS_IMETHODIMP
LoadInfo::GetIsFromProcessingFrameAttributes(
    bool* aIsFromProcessingFrameAttributes) {
  MOZ_ASSERT(aIsFromProcessingFrameAttributes);
  *aIsFromProcessingFrameAttributes = mIsFromProcessingFrameAttributes;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::GetResultPrincipalURI(nsIURI** aURI) {
  NS_IF_ADDREF(*aURI = mResultPrincipalURI);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetResultPrincipalURI(nsIURI* aURI) {
  mResultPrincipalURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetRequestBlockingReason(uint32_t aReason) {
  mRequestBlockingReason = aReason;
  return NS_OK;
}
NS_IMETHODIMP
LoadInfo::GetRequestBlockingReason(uint32_t* aReason) {
  *aReason = mRequestBlockingReason;
  return NS_OK;
}

void LoadInfo::SetClientInfo(const ClientInfo& aClientInfo) {
  mClientInfo.emplace(aClientInfo);
}

const Maybe<ClientInfo>& LoadInfo::GetClientInfo() { return mClientInfo; }

void LoadInfo::GiveReservedClientSource(
    UniquePtr<ClientSource>&& aClientSource) {
  MOZ_DIAGNOSTIC_ASSERT(aClientSource);
  mReservedClientSource = std::move(aClientSource);
  SetReservedClientInfo(mReservedClientSource->Info());
}

UniquePtr<ClientSource> LoadInfo::TakeReservedClientSource() {
  if (mReservedClientSource) {
    // If the reserved ClientInfo was set due to a ClientSource being present,
    // then clear that info object when the ClientSource is taken.
    mReservedClientInfo.reset();
  }
  return std::move(mReservedClientSource);
}

void LoadInfo::SetReservedClientInfo(const ClientInfo& aClientInfo) {
  MOZ_DIAGNOSTIC_ASSERT(mInitialClientInfo.isNothing());
  // Treat assignments of the same value as a no-op.  The emplace below
  // will normally assert when overwriting an existing value.
  if (mReservedClientInfo.isSome() &&
      mReservedClientInfo.ref() == aClientInfo) {
    return;
  }
  mReservedClientInfo.emplace(aClientInfo);
}

void LoadInfo::OverrideReservedClientInfoInParent(
    const ClientInfo& aClientInfo) {
  // This should only be called to handle redirects in the parent process.
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);

  mInitialClientInfo.reset();
  mReservedClientInfo.reset();
  mReservedClientInfo.emplace(aClientInfo);
}

const Maybe<ClientInfo>& LoadInfo::GetReservedClientInfo() {
  return mReservedClientInfo;
}

void LoadInfo::SetInitialClientInfo(const ClientInfo& aClientInfo) {
  MOZ_DIAGNOSTIC_ASSERT(!mReservedClientSource);
  MOZ_DIAGNOSTIC_ASSERT(mReservedClientInfo.isNothing());
  // Treat assignments of the same value as a no-op.  The emplace below
  // will normally assert when overwriting an existing value.
  if (mInitialClientInfo.isSome() && mInitialClientInfo.ref() == aClientInfo) {
    return;
  }
  mInitialClientInfo.emplace(aClientInfo);
}

const Maybe<ClientInfo>& LoadInfo::GetInitialClientInfo() {
  return mInitialClientInfo;
}

void LoadInfo::SetController(const ServiceWorkerDescriptor& aServiceWorker) {
  mController.emplace(aServiceWorker);
}

void LoadInfo::ClearController() { mController.reset(); }

const Maybe<ServiceWorkerDescriptor>& LoadInfo::GetController() {
  return mController;
}

void LoadInfo::SetPerformanceStorage(PerformanceStorage* aPerformanceStorage) {
  mPerformanceStorage = aPerformanceStorage;
}

PerformanceStorage* LoadInfo::GetPerformanceStorage() {
  return mPerformanceStorage;
}

NS_IMETHODIMP
LoadInfo::GetCspEventListener(nsICSPEventListener** aCSPEventListener) {
  NS_IF_ADDREF(*aCSPEventListener = mCSPEventListener);
  return NS_OK;
}

NS_IMETHODIMP
LoadInfo::SetCspEventListener(nsICSPEventListener* aCSPEventListener) {
  mCSPEventListener = aCSPEventListener;
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
