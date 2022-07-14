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
#include "ExtensionBrowser.h"
#include "ExtensionEventManager.h"
#include "ExtensionPort.h"

#include "mozilla/ConsoleReportCollector.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/SerializedStackHolder.h"
#include "mozilla/dom/FunctionBinding.h"

#include "js/CallAndConstruct.h"  // JS::IsCallable

namespace mozilla {
namespace extensions {

// ChromeCompatCallbackHandler

NS_IMPL_ISUPPORTS0(ChromeCompatCallbackHandler)

// static
void ChromeCompatCallbackHandler::Create(
    ExtensionBrowser* aExtensionBrowser, dom::Promise* aPromise,
    const RefPtr<dom::Function>& aCallback) {
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(aExtensionBrowser);
  MOZ_ASSERT(aCallback);

  RefPtr<ChromeCompatCallbackHandler> handler =
      new ChromeCompatCallbackHandler(aExtensionBrowser, aCallback);

  aPromise->AppendNativeHandler(handler);
}

void ChromeCompatCallbackHandler::ResolvedCallback(JSContext* aCx,
                                                   JS::Handle<JS::Value> aValue,
                                                   ErrorResult& aRv) {
  JS::Rooted<JS::Value> retval(aCx);
  IgnoredErrorResult rv;
  MOZ_KnownLive(mCallback)->Call({aValue}, &retval, rv);
}

void ChromeCompatCallbackHandler::RejectedCallback(JSContext* aCx,
                                                   JS::Handle<JS::Value> aValue,
                                                   ErrorResult& aRv) {
  JS::Rooted<JS::Value> retval(aCx);
  IgnoredErrorResult rv;
  // Call the chrome-compatible callback without any parameter, the errors
  // isn't passed to the callback as a parameter but the extension will be
  // able to retrieve it from chrome.runtime.lastError.
  mExtensionBrowser->SetLastError(aValue);
  MOZ_KnownLive(mCallback)->Call({}, &retval, rv);
  if (mExtensionBrowser->ClearLastError()) {
    ReportUncheckedLastError(aCx, aValue);
  }
}

void ChromeCompatCallbackHandler::ReportUncheckedLastError(
    JSContext* aCx, JS::Handle<JS::Value> aValue) {
  nsCString sourceSpec;
  uint32_t line = 0;
  uint32_t column = 0;
  nsString valueString;

  nsContentUtils::ExtractErrorValues(aCx, aValue, sourceSpec, &line, &column,
                                     valueString);

  nsTArray<nsString> params;
  params.AppendElement(valueString);

  RefPtr<ConsoleReportCollector> reporter = new ConsoleReportCollector();
  reporter->AddConsoleReport(nsIScriptError::errorFlag, "content javascript"_ns,
                             nsContentUtils::eDOM_PROPERTIES, sourceSpec, line,
                             column, "WebExtensionUncheckedLastError"_ns,
                             params);

  dom::WorkerPrivate* workerPrivate = dom::GetWorkerPrivateFromContext(aCx);
  RefPtr<Runnable> r = NS_NewRunnableFunction(
      "ChromeCompatCallbackHandler::ReportUncheckedLastError",
      [reporter]() { reporter->FlushReportsToConsole(0); });
  workerPrivate->DispatchToMainThread(r.forget());
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

void ExtensionAPIBase::CallWebExtMethodReturnsString(
    JSContext* aCx, const nsAString& aApiMethod,
    const dom::Sequence<JS::Value>& aArgs, nsAString& aRetVal,
    ErrorResult& aRv) {
  JS::Rooted<JS::Value> retval(aCx);
  auto request = CallSyncFunction(aApiMethod);
  request->Run(GetGlobalObject(), aCx, aArgs, &retval, aRv);
  if (aRv.Failed()) {
    return;
  }

  if (NS_WARN_IF(!retval.isString())) {
    ThrowUnexpectedError(aCx, aRv);
    return;
  }

  nsAutoJSString str;
  if (!str.init(aCx, retval.toString())) {
    JS_ClearPendingException(aCx);
    ThrowUnexpectedError(aCx, aRv);
    return;
  }

  aRetVal = str;
}

already_AddRefed<ExtensionPort> ExtensionAPIBase::CallWebExtMethodReturnsPort(
    JSContext* aCx, const nsAString& aApiMethod,
    const dom::Sequence<JS::Value>& aArgs, ErrorResult& aRv) {
  JS::Rooted<JS::Value> apiResult(aCx);
  auto request = CallSyncFunction(aApiMethod);
  request->Run(GetGlobalObject(), aCx, aArgs, &apiResult, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  IgnoredErrorResult rv;
  auto* extensionBrowser = GetExtensionBrowser();
  RefPtr<ExtensionPort> port = extensionBrowser->GetPort(apiResult, rv);
  if (NS_WARN_IF(rv.Failed())) {
    // ExtensionPort::Create doesn't throw the js exception with the generic
    // error message as the "api request forwarding" helper classes.
    ThrowUnexpectedError(aCx, aRv);
    return nullptr;
  }

  return port.forget();
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
    ChromeCompatCallbackHandler::Create(GetExtensionBrowser(), domPromise,
                                        aCallback);
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

void ExtensionAPIBase::GetWebExtPropertyAsString(const nsString& aPropertyName,
                                                 dom::DOMString& aRetval) {
  IgnoredErrorResult rv;

  dom::AutoJSAPI jsapi;
  auto* global = GetGlobalObject();

  if (!jsapi.Init(global)) {
    NS_WARNING("GetWebExtPropertyAsString fail to init jsapi");
    return;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> retval(cx);

  RefPtr<ExtensionAPIGetProperty> request = GetProperty(aPropertyName);
  request->Run(global, cx, &retval, rv);
  if (rv.Failed()) {
    NS_WARNING("GetWebExtPropertyAsString failure");
    return;
  }
  nsAutoJSString strRetval;
  if (!retval.isString() || !strRetval.init(cx, retval)) {
    NS_WARNING("GetWebExtPropertyAsString got a non string result");
    return;
  }
  aRetval.SetKnownLiveString(strRetval);
}

void ExtensionAPIBase::GetWebExtPropertyAsJSValue(
    JSContext* aCx, const nsAString& aPropertyName,
    JS::MutableHandle<JS::Value> aRetval) {
  IgnoredErrorResult rv;
  RefPtr<ExtensionAPIGetProperty> request = GetProperty(aPropertyName);
  request->Run(GetGlobalObject(), aCx, aRetval, rv);
  if (rv.Failed()) {
    NS_WARNING("GetWebExtPropertyAsJSValue failure");
    return;
  }
}

already_AddRefed<ExtensionEventManager> ExtensionAPIBase::CreateEventManager(
    const nsAString& aEventName) {
  RefPtr<ExtensionEventManager> eventMgr = new ExtensionEventManager(
      GetGlobalObject(), GetExtensionBrowser(), GetAPINamespace(), aEventName,
      GetAPIObjectType(), GetAPIObjectId());
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
