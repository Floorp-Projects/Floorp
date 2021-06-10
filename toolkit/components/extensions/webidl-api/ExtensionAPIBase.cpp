/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionAPIBase.h"

#include "ExtensionAPIRequestForwarder.h"
#include "ExtensionAPIAddRemoveListener.h"
#include "ExtensionAPICallAsyncFunction.h"
#include "ExtensionAPICallFunctionNoReturn.h"
#include "ExtensionAPICallSyncFunction.h"
#include "ExtensionAPIGetProperty.h"
#include "ExtensionEventManager.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/SerializedStackHolder.h"
#include "mozilla/dom/FunctionBinding.h"

namespace mozilla {
namespace extensions {

// ChromeCompatCallbackHandler

NS_IMPL_ISUPPORTS0(ChromeCompatCallbackHandler)

// static
void ChromeCompatCallbackHandler::Create(
    dom::Promise* aPromise, const RefPtr<dom::Function>& aCallback) {
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(aCallback);

  RefPtr<ChromeCompatCallbackHandler> handler =
      new ChromeCompatCallbackHandler(aCallback);

  aPromise->AppendNativeHandler(handler);
}

void ChromeCompatCallbackHandler::ResolvedCallback(
    JSContext* aCx, JS::Handle<JS::Value> aValue) {
  JS::RootedValue retval(aCx);
  IgnoredErrorResult rv;
  MOZ_KnownLive(mCallback)->Call({aValue}, &retval, rv);
}

void ChromeCompatCallbackHandler::RejectedCallback(
    JSContext* aCx, JS::Handle<JS::Value> aValue) {
  JS::RootedValue retval(aCx);
  IgnoredErrorResult rv;
  // Call the chrome-compatible callback without any parameter, the errors
  // isn't passed to the callback as a parameter but the extension will be
  // able to retrieve it from chrome.runtime.lastError.
  MOZ_KnownLive(mCallback)->Call({}, &retval, rv);
}

// WebExtensionStub methods shared between multiple API namespaces.

void ExtensionAPIBase::CallWebExtMethodNotImplementedNoReturn(
    JSContext* aCx, const nsAString& aApiMethod,
    const dom::Sequence<JS::Value>& aArgs, ErrorResult& aRv) {
  aRv.ThrowNotSupportedError("Not implemented");
}

void ExtensionAPIBase::CallWebExtMethodNotImplementedAsync(
    JSContext* aCx, const nsAString& aApiMethod,
    const dom::Sequence<JS::Value>& aArgs,
    const dom::Optional<OwningNonNull<dom::Function>>& aCallback,
    JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv) {
  CallWebExtMethodNotImplementedNoReturn(aCx, aApiMethod, aArgs, aRv);
}

void ExtensionAPIBase::CallWebExtMethodNotImplemented(
    JSContext* aCx, const nsAString& aApiMethod,
    const dom::Sequence<JS::Value>& aArgs, JS::MutableHandle<JS::Value> aRetval,
    ErrorResult& aRv) {
  CallWebExtMethodNotImplementedNoReturn(aCx, aApiMethod, aArgs, aRv);
}

void ExtensionAPIBase::CallWebExtMethodNoReturn(
    JSContext* aCx, const nsAString& aApiMethod,
    const dom::Sequence<JS::Value>& aArgs, ErrorResult& aRv) {
  auto request = CallFunctionNoReturn(aApiMethod);
  request->Run(GetGlobalObject(), aCx, aArgs, aRv);
  if (aRv.Failed()) {
    return;
  }
}

void ExtensionAPIBase::CallWebExtMethod(JSContext* aCx,
                                        const nsAString& aApiMethod,
                                        const dom::Sequence<JS::Value>& aArgs,
                                        JS::MutableHandle<JS::Value> aRetVal,
                                        ErrorResult& aRv) {
  auto request = CallSyncFunction(aApiMethod);
  request->Run(GetGlobalObject(), aCx, aArgs, aRetVal, aRv);
  if (aRv.Failed()) {
    return;
  }
}

void ExtensionAPIBase::CallWebExtMethodAsyncInternal(
    JSContext* aCx, const nsAString& aApiMethod,
    const dom::Sequence<JS::Value>& aArgs,
    const RefPtr<dom::Function>& aCallback,
    JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv) {
  auto* global = GetGlobalObject();

  IgnoredErrorResult erv;
  RefPtr<dom::Promise> domPromise = dom::Promise::Create(global, erv);
  if (NS_WARN_IF(erv.Failed())) {
    ThrowUnexpectedError(aCx, aRv);
    return;
  }
  MOZ_ASSERT(domPromise);
  auto request = CallAsyncFunction(aApiMethod);
  request->Run(global, aCx, aArgs, domPromise, aRv);
  if (aRv.Failed()) {
    return;
  }

  // The async method has been called with the chrome-compatible callback
  // convention.
  if (aCallback) {
    ChromeCompatCallbackHandler::Create(domPromise, aCallback);
    return;
  }

  if (NS_WARN_IF(!ToJSValue(aCx, domPromise, aRetval))) {
    ThrowUnexpectedError(aCx, aRv);
    return;
  }
}

void ExtensionAPIBase::CallWebExtMethodAsync(
    JSContext* aCx, const nsAString& aApiMethod,
    const dom::Sequence<JS::Value>& aArgs,
    const dom::Optional<OwningNonNull<dom::Function>>& aCallback,
    JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv) {
  RefPtr<dom::Function> callback = nullptr;
  if (aCallback.WasPassed()) {
    callback = &aCallback.Value();
  }
  CallWebExtMethodAsyncInternal(aCx, aApiMethod, aArgs, callback, aRetval, aRv);
}

void ExtensionAPIBase::CallWebExtMethodAsyncAmbiguous(
    JSContext* aCx, const nsAString& aApiMethod,
    const dom::Sequence<JS::Value>& aArgs, JS::MutableHandle<JS::Value> aRetval,
    ErrorResult& aRv) {
  RefPtr<dom::Function> chromeCompatCb;
  auto lastElement =
      aArgs.IsEmpty() ? JS::UndefinedValue() : aArgs.LastElement();
  dom::Sequence<JS::Value> callArgs(aArgs);
  if (lastElement.isObject() && JS::IsCallable(&lastElement.toObject())) {
    JS::Rooted<JSObject*> tempRoot(aCx, &lastElement.toObject());
    JS::Rooted<JSObject*> tempGlobalRoot(aCx, JS::CurrentGlobalOrNull(aCx));
    chromeCompatCb = new dom::Function(aCx, tempRoot, tempGlobalRoot,
                                       dom::GetIncumbentGlobal());

    Unused << callArgs.PopLastElement();
  }
  CallWebExtMethodAsyncInternal(aCx, aApiMethod, callArgs, chromeCompatCb,
                                aRetval, aRv);
}

// ExtensionAPIBase - API Request helpers

already_AddRefed<ExtensionEventManager> ExtensionAPIBase::CreateEventManager(
    const nsAString& aEventName) {
  RefPtr<ExtensionEventManager> eventMgr = new ExtensionEventManager(
      GetGlobalObject(), GetAPINamespace(), aEventName, GetAPIObjectType(),
      GetAPIObjectId());
  return eventMgr.forget();
}

RefPtr<ExtensionAPICallFunctionNoReturn> ExtensionAPIBase::CallFunctionNoReturn(
    const nsAString& aApiMethod) {
  return new ExtensionAPICallFunctionNoReturn(
      GetAPINamespace(), aApiMethod, GetAPIObjectType(), GetAPIObjectId());
}

RefPtr<ExtensionAPICallSyncFunction> ExtensionAPIBase::CallSyncFunction(
    const nsAString& aApiMethod) {
  return new ExtensionAPICallSyncFunction(GetAPINamespace(), aApiMethod,
                                          GetAPIObjectType(), GetAPIObjectId());
}

RefPtr<ExtensionAPICallAsyncFunction> ExtensionAPIBase::CallAsyncFunction(
    const nsAString& aApiMethod) {
  return new ExtensionAPICallAsyncFunction(
      GetAPINamespace(), aApiMethod, GetAPIObjectType(), GetAPIObjectId());
}

RefPtr<ExtensionAPIGetProperty> ExtensionAPIBase::GetProperty(
    const nsAString& aApiProperty) {
  return new ExtensionAPIGetProperty(GetAPINamespace(), aApiProperty,
                                     GetAPIObjectType(), GetAPIObjectId());
}

RefPtr<ExtensionAPIAddRemoveListener> ExtensionAPIBase::SendAddListener(
    const nsAString& aEventName) {
  using EType = ExtensionAPIAddRemoveListener::EType;
  return new ExtensionAPIAddRemoveListener(
      EType::eAddListener, GetAPINamespace(), aEventName, GetAPIObjectType(),
      GetAPIObjectId());
}

RefPtr<ExtensionAPIAddRemoveListener> ExtensionAPIBase::SendRemoveListener(
    const nsAString& aEventName) {
  using EType = ExtensionAPIAddRemoveListener::EType;
  return new ExtensionAPIAddRemoveListener(
      EType::eRemoveListener, GetAPINamespace(), aEventName, GetAPIObjectType(),
      GetAPIObjectId());
}

// static
void ExtensionAPIBase::ThrowUnexpectedError(JSContext* aCx, ErrorResult& aRv) {
  ExtensionAPIRequestForwarder::ThrowUnexpectedError(aCx, aRv);
}

}  // namespace extensions
}  // namespace mozilla
