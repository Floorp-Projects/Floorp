/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSecureBrowserUI.h"

#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/Document.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsITransportSecurityInfo.h"
#include "nsIWebProgress.h"
#include "nsNetUtil.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/Element.h"
#include "nsIBrowser.h"

using namespace mozilla;
using namespace mozilla::dom;

LazyLogModule gSecureBrowserUILog("nsSecureBrowserUI");

nsSecureBrowserUI::nsSecureBrowserUI(CanonicalBrowsingContext* aBrowsingContext)
    : mState(0) {
  MOZ_ASSERT(NS_IsMainThread());

  // The BrowsingContext will own the SecureBrowserUI object, we keep a weak
  // ref.
  mBrowsingContextId = aBrowsingContext->Id();
}

NS_IMPL_ISUPPORTS(nsSecureBrowserUI, nsISecureBrowserUI,
                  nsISupportsWeakReference)

NS_IMETHODIMP
nsSecureBrowserUI::GetState(uint32_t* aState) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aState);

  MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug,
          ("GetState %p mState: %x", this, mState));
  *aState = mState;
  return NS_OK;
}

void nsSecureBrowserUI::RecomputeSecurityFlags() {
  // Our BrowsingContext either has a new WindowGlobalParent, or the
  // existing one has mutated its security state.
  // Recompute our security state and fire notifications to listeners

  RefPtr<WindowGlobalParent> win = GetCurrentWindow();
  mState = nsIWebProgressListener::STATE_IS_INSECURE;

  // Only https is considered secure (it is possible to have e.g. an http URI
  // with a channel that has a securityInfo that indicates the connection is
  // secure - e.g. h2/alt-svc or by visiting an http URI over an https proxy).
  nsCOMPtr<nsITransportSecurityInfo> securityInfo;
  if (win && win->GetIsSecure()) {
    securityInfo = win->GetSecurityInfo();
    if (securityInfo) {
      MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug,
              ("  we have a security info %p", securityInfo.get()));

      nsresult rv = securityInfo->GetSecurityState(&mState);

      // If the security state is STATE_IS_INSECURE, the TLS handshake never
      // completed. Don't set any further state.
      if (NS_SUCCEEDED(rv) &&
          mState != nsIWebProgressListener::STATE_IS_INSECURE) {
        MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug,
                ("  set mTopLevelSecurityInfo"));
        bool isEV;
        rv = securityInfo->GetIsExtendedValidation(&isEV);
        if (NS_SUCCEEDED(rv) && isEV) {
          MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug, ("  is EV"));
          mState |= nsIWebProgressListener::STATE_IDENTITY_EV_TOPLEVEL;
        }
      }
    }
  }

  // Add upgraded-state flags when request has been
  // upgraded with HTTPS-Only Mode
  if (win) {
    // Check if top-level load has been upgraded
    uint32_t httpsOnlyStatus = win->HttpsOnlyStatus();
    if (!(httpsOnlyStatus & nsILoadInfo::HTTPS_ONLY_UNINITIALIZED) &&
        !(httpsOnlyStatus & nsILoadInfo::HTTPS_ONLY_EXEMPT)) {
      mState |= nsIWebProgressListener::STATE_HTTPS_ONLY_MODE_UPGRADED;
    }
    if (httpsOnlyStatus & nsILoadInfo::HTTPS_ONLY_UPGRADED_HTTPS_FIRST) {
      if (win->GetDocumentURI()->SchemeIs("https")) {
        mState |= nsIWebProgressListener::STATE_HTTPS_ONLY_MODE_UPGRADED_FIRST;
      } else {
        mState |= nsIWebProgressListener::STATE_HTTPS_ONLY_MODE_UPGRADE_FAILED;
      }
    }
    // Add the secruity flags from the window
    mState |= win->GetSecurityFlags();
  }

  // If we have loaded mixed content and this is a secure page,
  // then clear secure flags and add broken instead.
  static const uint32_t kLoadedMixedContentFlags =
      nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT |
      nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT;
  if (win && win->GetIsSecure() && (mState & kLoadedMixedContentFlags)) {
    // reset state security flag
    mState = mState >> 4 << 4;
    // set state security flag to broken, since there is mixed content
    mState |= nsIWebProgressListener::STATE_IS_BROKEN;
  }

  RefPtr<CanonicalBrowsingContext> ctx =
      CanonicalBrowsingContext::Get(mBrowsingContextId);
  if (!ctx) {
    return;
  }

  if (ctx->GetDocShell()) {
    nsDocShell* nativeDocShell = nsDocShell::Cast(ctx->GetDocShell());
    nativeDocShell->nsDocLoader::OnSecurityChange(nullptr, mState);
  } else if (ctx->GetWebProgress()) {
    ctx->GetWebProgress()->OnSecurityChange(nullptr, nullptr, mState);
  }
}

NS_IMETHODIMP
nsSecureBrowserUI::GetIsSecureContext(bool* aIsSecureContext) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aIsSecureContext);

  if (WindowGlobalParent* parent = GetCurrentWindow()) {
    *aIsSecureContext = parent->GetIsSecureContext();
  } else {
    *aIsSecureContext = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUI::GetSecInfo(nsITransportSecurityInfo** result) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(result);

  if (WindowGlobalParent* parent = GetCurrentWindow()) {
    *result = parent->GetSecurityInfo();
  }
  NS_IF_ADDREF(*result);

  return NS_OK;
}

WindowGlobalParent* nsSecureBrowserUI::GetCurrentWindow() {
  RefPtr<CanonicalBrowsingContext> ctx =
      CanonicalBrowsingContext::Get(mBrowsingContextId);
  if (!ctx) {
    return nullptr;
  }
  return ctx->GetCurrentWindowGlobal();
}
