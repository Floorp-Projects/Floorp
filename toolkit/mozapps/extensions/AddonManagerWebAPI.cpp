/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AddonManagerWebAPI.h"

#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/NavigatorBinding.h"

#include "mozilla/Preferences.h"
#include "nsGlobalWindow.h"

#include "nsIDocShell.h"
#include "nsIScriptObjectPrincipal.h"

namespace mozilla {
using namespace mozilla::dom;

// Checks if the given uri is secure and matches one of the hosts allowed to
// access the API.
bool
AddonManagerWebAPI::IsValidSite(nsIURI* uri)
{
  if (!uri) {
    return false;
  }

  bool isSecure;
  nsresult rv = uri->SchemeIs("https", &isSecure);
  if (NS_FAILED(rv) || !isSecure) {
    return false;
  }

  nsCString host;
  rv = uri->GetHost(host);
  if (NS_FAILED(rv)) {
    return false;
  }

  if (host.Equals("addons.mozilla.org") ||
      host.Equals("services.addons.mozilla.org")) {
    return true;
  }

  // When testing allow access to the developer sites.
  if (Preferences::GetBool("extensions.webapi.testing", false)) {
    if (host.Equals("addons.allizom.org") ||
        host.Equals("services.addons.allizom.org") ||
        host.Equals("addons-dev.allizom.org") ||
        host.Equals("services.addons-dev.allizom.org") ||
        host.Equals("example.com")) {
      return true;
    }
  }

  return false;
}

bool
AddonManagerWebAPI::IsAPIEnabled(JSContext* cx, JSObject* obj)
{
  nsGlobalWindow* global = xpc::WindowGlobalOrNull(obj);
  if (!global) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindowInner> win = global->AsInner();
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
    if (principal->GetIsSystemPrincipal()) {
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
    // mozbrowser or chrome boundaries.
    nsCOMPtr<nsIDocShellTreeItem> parent;
    nsresult rv = docShell->GetSameTypeParent(getter_AddRefs(parent));
    if (NS_FAILED(rv)) {
      return false;
    }

    if (!parent) {
      // No parent means we've hit a mozbrowser or chrome boundary so allow
      // access to the API.
      return true;
    }

    nsIDocument* doc = win->GetDoc();
    if (!doc) {
      return false;
    }

    doc = doc->GetParentDocument();
    if (!doc) {
      // Getting here means something has been torn down so fail safe.
      return false;
    }


    win = doc->GetInnerWindow();
  }

  // Found a document with no inner window, don't grant access to the API.
  return false;
}

} // namespace mozilla
