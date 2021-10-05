/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionBrowser.h"

#include "mozilla/dom/ExtensionBrowserBinding.h"
#include "mozilla/dom/WorkerPrivate.h"  // GetWorkerPrivateFromContext
#include "mozilla/extensions/ExtensionMockAPI.h"
#include "mozilla/extensions/WebExtensionPolicy.h"

namespace mozilla {
namespace extensions {

NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionBrowser);
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionBrowser)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ExtensionBrowser, mGlobal,
                                      mExtensionMockAPI);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionBrowser)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ExtensionBrowser::ExtensionBrowser(nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal) {
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);
}

JSObject* ExtensionBrowser::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return dom::ExtensionBrowser_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* ExtensionBrowser::GetParentObject() const { return mGlobal; }

bool ExtensionAPIAllowed(JSContext* aCx, JSObject* aGlobal) {
#ifdef MOZ_WEBEXT_WEBIDL_ENABLED
  // Only expose the Extension API bindings if:
  // - the context is related to a worker where the Extension API are allowed
  //   (currently only the extension service worker declared in the extension
  //   manifest met this condition)
  // - the global is an extension window or an extension content script sandbox
  //   TODO:
  //   - the support for the extension window is deferred to a followup.
  //   - support for the content script sandboxes is also deferred to follow-ups
  //   - lock native Extension API in an extension window or sandbox behind a
  //     separate pref.
  MOZ_DIAGNOSTIC_ASSERT(
      !NS_IsMainThread(),
      "ExtensionAPI webidl bindings does not yet support main thread globals");

  // Verify if the Extensions API should be allowed on a worker thread.
  if (!StaticPrefs::extensions_backgroundServiceWorker_enabled_AtStartup()) {
    return false;
  }

  auto* workerPrivate = mozilla::dom::GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);
  MOZ_ASSERT(workerPrivate->IsServiceWorker());

  return workerPrivate->ExtensionAPIAllowed();
#else
  // Always return false on build where MOZ_WEBEXT_WEBIDL_ENABLED is set to
  // false (currently on all channels but nightly).
  return false;
#endif
}

ExtensionMockAPI* ExtensionBrowser::GetExtensionMockAPI() {
  if (!mExtensionMockAPI) {
    mExtensionMockAPI = new ExtensionMockAPI(mGlobal, this);
  }

  return mExtensionMockAPI;
}

}  // namespace extensions
}  // namespace mozilla
