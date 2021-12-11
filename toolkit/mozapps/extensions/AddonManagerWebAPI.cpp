/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AddonManagerWebAPI.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/NavigatorBinding.h"

#include "mozilla/Preferences.h"
#include "nsGlobalWindow.h"
#include "xpcpublic.h"

#include "nsIDocShell.h"
#include "nsIScriptObjectPrincipal.h"

namespace mozilla {
using namespace mozilla::dom;

static bool IsValidHost(const nsACString& host) {
  // This hidden pref allows users to disable mozAddonManager entirely if they
  // want for fingerprinting resistance. Someone like Tor browser will use this
  // pref.
  if (Preferences::GetBool(
          "privacy.resistFingerprinting.block_mozAddonManager")) {
    return false;
  }

  if (host.EqualsLiteral("addons.mozilla.org")) {
    return true;
  }

  // When testing allow access to the developer sites.
  if (Preferences::GetBool("extensions.webapi.testing", false)) {
    if (host.LowerCaseEqualsLiteral("addons.allizom.org") ||
        host.LowerCaseEqualsLiteral("addons-dev.allizom.org") ||
        host.LowerCaseEqualsLiteral("example.com")) {
      return true;
    }
  }

  return false;
}

// Checks if the given uri is secure and matches one of the hosts allowed to
// access the API.
bool AddonManagerWebAPI::IsValidSite(nsIURI* uri) {
  if (!uri) {
    return false;
  }

  if (!uri->SchemeIs("https")) {
    if (!(xpc::IsInAutomation() &&
          Preferences::GetBool("extensions.webapi.testing.http", false))) {
      return false;
    }
  }

  nsAutoCString host;
  nsresult rv = uri->GetHost(host);
  if (NS_FAILED(rv)) {
    return false;
  }

  return IsValidHost(host);
}

#ifndef ANDROID
bool AddonManagerWebAPI::IsAPIEnabled(JSContext* aCx, JSObject* aGlobal) {
  MOZ_DIAGNOSTIC_ASSERT(JS_IsGlobalObject(aGlobal));
  nsCOMPtr<nsPIDOMWindowInner> win = xpc::WindowOrNull(aGlobal);
  if (!win) {
    return false;
  }

  // Check that the current window and all parent frames are allowed access to
  // the API.
  while (win) {
    nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(win);
    if (!sop) {
      return false;
    }

    nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
    if (!principal) {
      return false;
    }

    // Reaching a window with a system principal means we have reached
    // privileged UI of some kind so stop at this point and allow access.
    if (principal->IsSystemPrincipal()) {
      return true;
    }

    nsCOMPtr<nsIDocShell> docShell = win->GetDocShell();
    if (!docShell) {
      // This window has been torn down so don't allow access to the API.
      return false;
    }

    if (!IsValidSite(win->GetDocumentURI())) {
      return false;
    }

    // Checks whether there is a parent frame of the same type. This won't cross
    // mozbrowser or chrome or fission/process boundaries.
    nsCOMPtr<nsIDocShellTreeItem> parent;
    nsresult rv = docShell->GetInProcessSameTypeParent(getter_AddRefs(parent));
    if (NS_FAILED(rv)) {
      return false;
    }

    // No parent means we've hit a mozbrowser or chrome or process boundary.
    if (!parent) {
      // With Fission, a cross-origin iframe has an out-of-process parent, but
      // DocShell knows nothing about it. We need to ask BrowsingContext here,
      // and only allow API access if AMO is actually at the top, not framed
      // by evilleagueofevil.com.
      return docShell->GetBrowsingContext()->IsTopContent();
    }

    Document* doc = win->GetDoc();
    if (!doc) {
      return false;
    }

    doc = doc->GetInProcessParentDocument();
    if (!doc) {
      // Getting here means something has been torn down so fail safe.
      return false;
    }

    win = doc->GetInnerWindow();
  }

  // Found a document with no inner window, don't grant access to the API.
  return false;
}
#else   // We don't support mozAddonManager on Android
bool AddonManagerWebAPI::IsAPIEnabled(JSContext* aCx, JSObject* aGlobal) {
  return false;
}
#endif  // ifndef ANDROID

namespace dom {

bool AddonManagerPermissions::IsHostPermitted(const GlobalObject& /*unused*/,
                                              const nsAString& host) {
  return IsValidHost(NS_ConvertUTF16toUTF8(host));
}

}  // namespace dom

}  // namespace mozilla
