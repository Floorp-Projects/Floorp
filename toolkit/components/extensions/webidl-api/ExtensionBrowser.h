/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionBrowser_h
#define mozilla_extensions_ExtensionBrowser_h

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace extensions {

class ExtensionAlarms;
class ExtensionMockAPI;
class ExtensionPort;
class ExtensionRuntime;
class ExtensionTest;

bool ExtensionAPIAllowed(JSContext* aCx, JSObject* aGlobal);

class ExtensionBrowser final : public nsISupports, public nsWrapperCache {
  nsCOMPtr<nsIGlobalObject> mGlobal;
  JS::Heap<JS::Value> mLastError;
  bool mCheckedLastError;
  RefPtr<ExtensionAlarms> mExtensionAlarms;
  RefPtr<ExtensionMockAPI> mExtensionMockAPI;
  RefPtr<ExtensionRuntime> mExtensionRuntime;
  RefPtr<ExtensionTest> mExtensionTest;
  nsTHashMap<nsStringHashKey, WeakPtr<ExtensionPort>> mPortsLookup;

  ~ExtensionBrowser() = default;

 public:
  explicit ExtensionBrowser(nsIGlobalObject* aGlobal);

  // Helpers used for the expected behavior of the browser.runtime.lastError
  // and browser.extension.lastError.
  void SetLastError(JS::Handle<JS::Value> aLastError);
  void GetLastError(JS::MutableHandle<JS::Value> aRetVal);
  // ClearLastError is used by ChromeCompatCallbackHandler::RejectedCallback
  // to clear the lastError property. When this method returns true the
  // caller will know that the error value wasn't checked by the callback and
  // should be reported to the console
  bool ClearLastError();

  // Helpers used for the ExtensionPort.

  // Get an ExtensionPort instance given its port descriptor (returns an
  // existing port if an instance is still tracked in the ports lookup table,
  // otherwise it creates a new one).
  already_AddRefed<ExtensionPort> GetPort(
      JS::Handle<JS::Value> aDescriptorValue, ErrorResult& aRv);

  // Remove the entry for an ExtensionPort tracked in the ports lookup map
  // given its portId (called from the ExtensionPort destructor when the
  // instance is being released).
  void ForgetReleasedPort(const nsAString& aPortId);

  // nsWrapperCache interface methods
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // DOM bindings methods

  nsIGlobalObject* GetParentObject() const;

  ExtensionAlarms* GetExtensionAlarms();
  ExtensionMockAPI* GetExtensionMockAPI();
  ExtensionRuntime* GetExtensionRuntime();
  ExtensionTest* GetExtensionTest();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ExtensionBrowser)
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionBrowser_h
