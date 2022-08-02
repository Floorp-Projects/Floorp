/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionBrowser_h
#define mozilla_extensions_ExtensionBrowser_h

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsTHashMap.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace extensions {

class ExtensionAlarms;
class ExtensionMockAPI;
class ExtensionPort;
class ExtensionRuntime;
class ExtensionScripting;
class ExtensionTest;

bool ExtensionAPIAllowed(JSContext* aCx, JSObject* aGlobal);

void CreateAndDispatchInitWorkerContextRunnable();

already_AddRefed<Runnable> CreateWorkerLoadedRunnable(
    const uint64_t aServiceWorkerDescriptorId,
    const nsCOMPtr<nsIURI>& aWorkerBaseURI);

already_AddRefed<Runnable> CreateWorkerDestroyedRunnable(
    const uint64_t aServiceWorkerDescriptorId,
    const nsCOMPtr<nsIURI>& aWorkerBaseURI);

// An HashMap used to keep track of listeners registered synchronously while
// the worker script is executing, used internally by nsIServiceWorkerManager
// wakeforExtensionAPIEvent method to resolve to true if the worker script
// spawned did have a listener subscribed for the related API event name.
class ExtensionEventWakeupMap final
    : public nsTHashMap<nsStringHashKey, uint64_t> {
  static void ToMapKey(const nsAString& aAPINamespace,
                       const nsAString& aAPIName, nsAString& aResultMapKey);

 public:
  nsresult IncrementListeners(const nsAString& aAPINamespace,
                              const nsAString& aAPIName);
  nsresult DecrementListeners(const nsAString& aAPINamespace,
                              const nsAString& aAPIName);
  bool HasListener(const nsAString& aAPINamespace, const nsAString& aAPIName);
};

class ExtensionBrowser final : public nsISupports, public nsWrapperCache {
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

  // Helpers used to keep track of the event listeners added during the
  // initial sync worker script execution.
  nsresult TrackWakeupEventListener(JSContext* aCx,
                                    const nsString& aAPINamespace,
                                    const nsString& aAPIName);
  nsresult UntrackWakeupEventListener(JSContext* aCx,
                                      const nsString& aAPINamespace,
                                      const nsString& aAPIName);
  bool HasWakeupEventListener(const nsString& aAPINamespace,
                              const nsString& aAPIName);

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
  ExtensionScripting* GetExtensionScripting();
  ExtensionTest* GetExtensionTest();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ExtensionBrowser)

 private:
  ~ExtensionBrowser() = default;

  nsCOMPtr<nsIGlobalObject> mGlobal;
  JS::Heap<JS::Value> mLastError;
  bool mCheckedLastError;
  RefPtr<ExtensionAlarms> mExtensionAlarms;
  RefPtr<ExtensionMockAPI> mExtensionMockAPI;
  RefPtr<ExtensionRuntime> mExtensionRuntime;
  RefPtr<ExtensionScripting> mExtensionScripting;
  RefPtr<ExtensionTest> mExtensionTest;
  nsTHashMap<nsStringHashKey, WeakPtr<ExtensionPort>> mPortsLookup;
  // `[APINamespace].[APIName]` => int64 (listeners count)
  ExtensionEventWakeupMap mExpectedEventWakeupMap;
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionBrowser_h
