/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionBrowser.h"

#include "mozilla/dom/ExtensionBrowserBinding.h"
#include "mozilla/dom/ExtensionPortBinding.h"  // ExtensionPortDescriptor
#include "mozilla/dom/WorkerScope.h"           // GetWorkerPrivateFromContext
#include "mozilla/extensions/ExtensionAlarms.h"
#include "mozilla/extensions/ExtensionMockAPI.h"
#include "mozilla/extensions/ExtensionPort.h"
#include "mozilla/extensions/ExtensionRuntime.h"
#include "mozilla/extensions/ExtensionScripting.h"
#include "mozilla/extensions/ExtensionTest.h"
#include "mozilla/extensions/WebExtensionPolicy.h"

namespace mozilla {
namespace extensions {

NS_IMPL_CYCLE_COLLECTION_CLASS(ExtensionBrowser)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionBrowser)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionBrowser)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionBrowser)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ExtensionBrowser)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mExtensionAlarms)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mExtensionMockAPI)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mExtensionRuntime)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mExtensionScripting)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mExtensionTest)
  tmp->mLastError.setUndefined();
  tmp->mPortsLookup.Clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ExtensionBrowser)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mExtensionAlarms)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mExtensionMockAPI)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mExtensionRuntime)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mExtensionScripting)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mExtensionTest)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ExtensionBrowser)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mLastError)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

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

void CreateAndDispatchInitWorkerContextRunnable() {
  MOZ_ASSERT(dom::IsCurrentThreadRunningWorker());
  // DO NOT pass this WorkerPrivate raw pointer to anything else but the
  // RequestInitWorkerRunnable (which extends dom::WorkerMainThreadRunnable).
  dom::WorkerPrivate* workerPrivate = dom::GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);
  MOZ_ASSERT(workerPrivate->ExtensionAPIAllowed());
  MOZ_ASSERT(workerPrivate->IsServiceWorker());
  workerPrivate->AssertIsOnWorkerThread();

  auto* workerScope = workerPrivate->GlobalScope();
  if (NS_WARN_IF(!workerScope)) {
    return;
  }

  Maybe<dom::ClientInfo> clientInfo = workerScope->GetClientInfo();
  if (NS_WARN_IF(clientInfo.isNothing())) {
    return;
  }

  RefPtr<RequestInitWorkerRunnable> runnable =
      new RequestInitWorkerRunnable(std::move(workerPrivate), clientInfo);
  IgnoredErrorResult rv;
  runnable->Dispatch(dom::WorkerStatus::Canceling, rv);
  if (rv.Failed()) {
    NS_WARNING("Failed to dispatch extensions::RequestInitWorkerRunnable");
  }
}

already_AddRefed<Runnable> CreateWorkerLoadedRunnable(
    const uint64_t aServiceWorkerDescriptorId,
    const nsCOMPtr<nsIURI>& aWorkerBaseURI) {
  RefPtr<NotifyWorkerLoadedRunnable> runnable = new NotifyWorkerLoadedRunnable(
      aServiceWorkerDescriptorId, aWorkerBaseURI);
  return runnable.forget();
}

already_AddRefed<Runnable> CreateWorkerDestroyedRunnable(
    const uint64_t aServiceWorkerDescriptorId,
    const nsCOMPtr<nsIURI>& aWorkerBaseURI) {
  RefPtr<NotifyWorkerDestroyedRunnable> runnable =
      new NotifyWorkerDestroyedRunnable(aServiceWorkerDescriptorId,
                                        aWorkerBaseURI);
  return runnable.forget();
}

void ExtensionBrowser::SetLastError(JS::Handle<JS::Value> aLastError) {
  mLastError.set(aLastError);
  mCheckedLastError = false;
}

void ExtensionBrowser::GetLastError(JS::MutableHandle<JS::Value> aRetVal) {
  aRetVal.set(mLastError);
  mCheckedLastError = true;
}

bool ExtensionBrowser::ClearLastError() {
  bool shouldReport = !mCheckedLastError;
  mLastError.setUndefined();
  return shouldReport;
}

already_AddRefed<ExtensionPort> ExtensionBrowser::GetPort(
    JS::Handle<JS::Value> aDescriptorValue, ErrorResult& aRv) {
  // Get a port descriptor from the js value got from the API request
  // handler.
  UniquePtr<dom::ExtensionPortDescriptor> portDescriptor =
      ExtensionPort::ToPortDescriptor(aDescriptorValue, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  auto portId = portDescriptor->mPortId;
  auto maybePort = mPortsLookup.MaybeGet(portId);
  if (maybePort.isSome() && maybePort.value().get()) {
    RefPtr<ExtensionPort> existingPort = maybePort.value().get();
    return existingPort.forget();
  }

  RefPtr<ExtensionPort> newPort =
      ExtensionPort::Create(mGlobal, this, std::move(portDescriptor));
  mPortsLookup.InsertOrUpdate(portId, newPort);
  return newPort.forget();
}

void ExtensionBrowser::ForgetReleasedPort(const nsAString& aPortId) {
  mPortsLookup.Remove(aPortId);
}

ExtensionAlarms* ExtensionBrowser::GetExtensionAlarms() {
  if (!mExtensionAlarms) {
    mExtensionAlarms = new ExtensionAlarms(mGlobal, this);
  }

  return mExtensionAlarms;
}

ExtensionMockAPI* ExtensionBrowser::GetExtensionMockAPI() {
  if (!mExtensionMockAPI) {
    mExtensionMockAPI = new ExtensionMockAPI(mGlobal, this);
  }

  return mExtensionMockAPI;
}

ExtensionRuntime* ExtensionBrowser::GetExtensionRuntime() {
  if (!mExtensionRuntime) {
    mExtensionRuntime = new ExtensionRuntime(mGlobal, this);
  }

  return mExtensionRuntime;
}

ExtensionScripting* ExtensionBrowser::GetExtensionScripting() {
  if (!mExtensionScripting) {
    mExtensionScripting = new ExtensionScripting(mGlobal, this);
  }

  return mExtensionScripting;
}

ExtensionTest* ExtensionBrowser::GetExtensionTest() {
  if (!mExtensionTest) {
    mExtensionTest = new ExtensionTest(mGlobal, this);
  }

  return mExtensionTest;
}

// static
void ExtensionEventWakeupMap::ToMapKey(const nsAString& aAPINamespace,
                                       const nsAString& aAPIName,
                                       nsAString& aResultMapKey) {
  aResultMapKey.Truncate();
  aResultMapKey.AppendPrintf("%s.%s",
                             NS_ConvertUTF16toUTF8(aAPINamespace).get(),
                             NS_ConvertUTF16toUTF8(aAPIName).get());
}

nsresult ExtensionEventWakeupMap::IncrementListeners(
    const nsAString& aAPINamespace, const nsAString& aAPIName) {
  nsString key;
  ToMapKey(aAPINamespace, aAPIName, key);
  auto maybeCount = MaybeGet(key);
  if (maybeCount.isSome()) {
    InsertOrUpdate(key, maybeCount.value() + 1);
  } else {
    InsertOrUpdate(key, 1);
  }

  return NS_OK;
}

nsresult ExtensionEventWakeupMap::DecrementListeners(
    const nsAString& aAPINamespace, const nsAString& aAPIName) {
  nsString key;
  ToMapKey(aAPINamespace, aAPIName, key);
  auto maybeCount = MaybeGet(key);
  if (maybeCount.isSome()) {
    MOZ_ASSERT(maybeCount.value() >= 1, "Unexpected counter value set to zero");
    uint64_t val = maybeCount.value() - 1;
    if (val == 0) {
      Remove(key);
    } else {
      InsertOrUpdate(key, val);
    }
  }

  return NS_OK;
}

bool ExtensionEventWakeupMap::HasListener(const nsAString& aAPINamespace,
                                          const nsAString& aAPIName) {
  nsString key;
  ToMapKey(aAPINamespace, aAPIName, key);
  auto maybeCount = MaybeGet(key);
  return (maybeCount.isSome() && maybeCount.value() > 0);
}

nsresult ExtensionBrowser::TrackWakeupEventListener(
    JSContext* aCx, const nsString& aAPINamespace, const nsString& aAPIName) {
  auto* workerPrivate = mozilla::dom::GetWorkerPrivateFromContext(aCx);
  if (workerPrivate->WorkerScriptExecutedSuccessfully()) {
    // Ignore if the worker script has already executed all its synchronous
    // statements.
    return NS_OK;
  }
  mExpectedEventWakeupMap.IncrementListeners(aAPINamespace, aAPIName);
  return NS_OK;
}

nsresult ExtensionBrowser::UntrackWakeupEventListener(
    JSContext* aCx, const nsString& aAPINamespace, const nsString& aAPIName) {
  auto* workerPrivate = mozilla::dom::GetWorkerPrivateFromContext(aCx);
  if (workerPrivate->WorkerScriptExecutedSuccessfully()) {
    // Ignore if the worker script has already executed all its synchronous
    return NS_OK;
  }
  mExpectedEventWakeupMap.DecrementListeners(aAPINamespace, aAPIName);
  return NS_OK;
}

bool ExtensionBrowser::HasWakeupEventListener(const nsString& aAPINamespace,
                                              const nsString& aAPIName) {
  return mExpectedEventWakeupMap.HasListener(aAPINamespace, aAPIName);
}

}  // namespace extensions
}  // namespace mozilla
