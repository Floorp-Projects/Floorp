/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AddonManagerWebAPI.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/NavigatorBinding.h"

#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_extensions.h"
#include "nsGlobalWindowInner.h"
#include "xpcpublic.h"

#include "nsIDocShell.h"
#include "nsIScriptObjectPrincipal.h"

namespace mozilla {
using namespace mozilla::dom;

#ifndef MOZ_THUNDERBIRD
#  define MOZ_AMO_HOSTNAME "addons.mozilla.org"
#  define MOZ_AMO_STAGE_HOSTNAME "addons.allizom.org"
#  define MOZ_AMO_DEV_HOSTNAME "addons-dev.allizom.org"
#else
#  define MOZ_AMO_HOSTNAME "addons.thunderbird.net"
#  define MOZ_AMO_STAGE_HOSTNAME "addons-stage.thunderbird.net"
#  undef MOZ_AMO_DEV_HOSTNAME
#endif

static bool IsValidHost(const nsACString& host) {
  // This hidden pref allows users to disable mozAddonManager entirely if they
  // want for fingerprinting resistance. Someone like Tor browser will use this
  // pref.
  if (StaticPrefs::privacy_resistFingerprinting_block_mozAddonManager()) {
    return false;
  }

  if (host.EqualsLiteral(MOZ_AMO_HOSTNAME)) {
    return true;
  }

  // When testing allow access to the developer sites.
  if (StaticPrefs::extensions_webapi_testing()) {
    if (host.LowerCaseEqualsLiteral(MOZ_AMO_STAGE_HOSTNAME) ||
#ifdef MOZ_AMO_DEV_HOSTNAME
        host.LowerCaseEqualsLiteral(MOZ_AMO_DEV_HOSTNAME) ||
#endif
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
          StaticPrefs::extensions_webapi_testing_http())) {
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

bool AddonManagerWebAPI::IsAPIEnabled(JSContext* aCx, JSObject* aGlobal) {
  if (!StaticPrefs::extensions_webapi_enabled()) {
    return false;
  }

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

namespace dom {

bool AddonManagerPermissions::IsHostPermitted(const GlobalObject& /*unused*/,
                                              const nsAString& host) {
  return IsValidHost(NS_ConvertUTF16toUTF8(host));
}

}  // namespace dom

}  // namespace mozilla
