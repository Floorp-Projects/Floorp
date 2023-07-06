/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionEventManager.h"

#include "ExtensionAPIAddRemoveListener.h"

#include "mozilla/dom/ExtensionEventManagerBinding.h"
#include "nsIGlobalObject.h"
#include "ExtensionEventListener.h"

namespace mozilla {
namespace extensions {

NS_IMPL_CYCLE_COLLECTION_CLASS(ExtensionEventManager);
NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionEventManager);
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionEventManager)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ExtensionEventManager)
  tmp->mListeners.clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mExtensionBrowser)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ExtensionEventManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mExtensionBrowser)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ExtensionEventManager)
  for (auto iter = tmp->mListeners.iter(); !iter.done(); iter.next()) {
    aCallbacks.Trace(&iter.get().mutableKey(), "mListeners key", aClosure);
  }
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionEventManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ExtensionEventManager::ExtensionEventManager(
    nsIGlobalObject* aGlobal, ExtensionBrowser* aExtensionBrowser,
    const nsAString& aNamespace, const nsAString& aEventName,
    const nsAString& aObjectType, const nsAString& aObjectId)
    : mGlobal(aGlobal),
      mExtensionBrowser(aExtensionBrowser),
      mAPINamespace(aNamespace),
      mEventName(aEventName),
      mAPIObjectType(aObjectType),
      mAPIObjectId(aObjectId) {
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);
  MOZ_DIAGNOSTIC_ASSERT(mExtensionBrowser);

  mozilla::HoldJSObjects(this);
}

ExtensionEventManager::~ExtensionEventManager() {
  ReleaseListeners();
  mozilla::DropJSObjects(this);
};

void ExtensionEventManager::ReleaseListeners() {
  if (mListeners.empty()) {
    return;
  }

  for (auto iter = mListeners.iter(); !iter.done(); iter.next()) {
    iter.get().value()->Cleanup();
  }

  mListeners.clear();
}

JSObject* ExtensionEventManager::WrapObject(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return dom::ExtensionEventManager_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* ExtensionEventManager::GetParentObject() const {
  return mGlobal;
}

void ExtensionEventManager::AddListener(
    JSContext* aCx, dom::Function& aCallback,
    const dom::Optional<JS::Handle<JSObject*>>& aOptions, ErrorResult& aRv) {
  JS::Rooted<JSObject*> cb(aCx, aCallback.CallbackOrNull());
  if (cb == nullptr) {
    ThrowUnexpectedError(aCx, aRv);
    return;
  }

  RefPtr<ExtensionEventManager> self = this;

  IgnoredErrorResult rv;
  RefPtr<ExtensionEventListener> wrappedCb = ExtensionEventListener::Create(
      mGlobal, mExtensionBrowser, &aCallback,
      [self = std::move(self)]() { self->ReleaseListeners(); }, rv);

  if (NS_WARN_IF(rv.Failed())) {
    ThrowUnexpectedError(aCx, aRv);
    return;
  }

  RefPtr<ExtensionEventListener> storedWrapper = wrappedCb;
  if (!mListeners.put(cb, std::move(storedWrapper))) {
    ThrowUnexpectedError(aCx, aRv);
    return;
  }

  auto request = SendAddListener(mEventName);
  request->Run(mGlobal, aCx, {}, wrappedCb, aRv);

  if (!aRv.Failed() && mAPIObjectType.IsEmpty()) {
    mExtensionBrowser->TrackWakeupEventListener(aCx, mAPINamespace, mEventName);
  }
}

void ExtensionEventManager::RemoveListener(dom::Function& aCallback,
                                           ErrorResult& aRv) {
  dom::AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mGlobal))) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> cb(cx, aCallback.CallbackOrNull());
  const auto& ptr = mListeners.lookup(cb);

  // Return earlier if the listener wasn't found
  if (!ptr) {
    return;
  }

  RefPtr<ExtensionEventListener> wrappedCb = ptr->value();
  auto request = SendRemoveListener(mEventName);
  request->Run(mGlobal, cx, {}, wrappedCb, aRv);

  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (mAPIObjectType.IsEmpty()) {
    mExtensionBrowser->UntrackWakeupEventListener(cx, mAPINamespace,
                                                  mEventName);
  }

  mListeners.remove(cb);

  wrappedCb->Cleanup();
}

bool ExtensionEventManager::HasListener(dom::Function& aCallback,
                                        ErrorResult& aRv) const {
  return mListeners.has(aCallback.CallbackOrNull());
}

bool ExtensionEventManager::HasListeners(ErrorResult& aRv) const {
  return !mListeners.empty();
}

}  // namespace extensions
}  // namespace mozilla
