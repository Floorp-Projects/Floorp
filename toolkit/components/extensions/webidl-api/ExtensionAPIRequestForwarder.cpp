/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionAPIRequestForwarder.h"
#include "ExtensionEventListener.h"

#include "js/Promise.h"
#include "js/PropertyAndElement.h"  // JS_GetElement
#include "mozilla/dom/Client.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ClonedErrorHolder.h"
#include "mozilla/dom/ClonedErrorHolderBinding.h"
#include "mozilla/dom/ExtensionBrowserBinding.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/SerializedStackHolder.h"
#include "mozilla/dom/ServiceWorkerInfo.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/dom/ServiceWorkerRegistrationInfo.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/ExtensionPolicyService.h"
#include "nsIGlobalObject.h"
#include "nsImportModule.h"
#include "nsIXPConnect.h"

namespace mozilla {
namespace extensions {

// ExtensionAPIRequestForwarder

// static
void ExtensionAPIRequestForwarder::ThrowUnexpectedError(JSContext* aCx,
                                                        ErrorResult& aRv) {
  aRv.MightThrowJSException();
  JS_ReportErrorASCII(aCx, "An unexpected error occurred");
  aRv.StealExceptionFromJSContext(aCx);
}

ExtensionAPIRequestForwarder::ExtensionAPIRequestForwarder(
    const mozIExtensionAPIRequest::RequestType aRequestType,
    const nsAString& aApiNamespace, const nsAString& aApiMethod,
    const nsAString& aApiObjectType, const nsAString& aApiObjectId) {
  mRequestType = aRequestType;
  mRequestTarget.mNamespace = aApiNamespace;
  mRequestTarget.mMethod = aApiMethod;
  mRequestTarget.mObjectType = aApiObjectType;
  mRequestTarget.mObjectId = aApiObjectId;
}

// static
nsresult ExtensionAPIRequestForwarder::JSArrayToSequence(
    JSContext* aCx, JS::Handle<JS::Value> aJSValue,
    dom::Sequence<JS::Value>& aResult) {
  bool isArray;
  JS::Rooted<JSObject*> obj(aCx, aJSValue.toObjectOrNull());

  if (NS_WARN_IF(!obj || !JS::IsArrayObject(aCx, obj, &isArray))) {
    return NS_ERROR_UNEXPECTED;
  }

  if (isArray) {
    uint32_t len;
    if (NS_WARN_IF(!JS::GetArrayLength(aCx, obj, &len))) {
      return NS_ERROR_UNEXPECTED;
    }

    for (uint32_t i = 0; i < len; i++) {
      JS::Rooted<JS::Value> v(aCx);
      JS_GetElement(aCx, obj, i, &v);
      if (NS_WARN_IF(!aResult.AppendElement(v, fallible))) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  } else if (NS_WARN_IF(!aResult.AppendElement(aJSValue, fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

/* static */
mozIExtensionAPIRequestHandler&
ExtensionAPIRequestForwarder::APIRequestHandler() {
  static nsCOMPtr<mozIExtensionAPIRequestHandler> sAPIRequestHandler;

  MOZ_ASSERT(NS_IsMainThread());

  if (MOZ_UNLIKELY(!sAPIRequestHandler)) {
    sAPIRequestHandler =
        do_ImportModule("resource://gre/modules/ExtensionProcessScript.jsm",
                        "ExtensionAPIRequestHandler");
    MOZ_RELEASE_ASSERT(sAPIRequestHandler);
    ClearOnShutdown(&sAPIRequestHandler);
  }
  return *sAPIRequestHandler;
}

void ExtensionAPIRequestForwarder::SetSerializedCallerStack(
    UniquePtr<dom::SerializedStackHolder> aCallerStack) {
  MOZ_ASSERT(dom::IsCurrentThreadRunningWorker());
  MOZ_ASSERT(mStackHolder.isNothing());
  mStackHolder = Some(std::move(aCallerStack));
}

void ExtensionAPIRequestForwarder::Run(nsIGlobalObject* aGlobal, JSContext* aCx,
                                       const dom::Sequence<JS::Value>& aArgs,
                                       ExtensionEventListener* aListener,
                                       JS::MutableHandle<JS::Value> aRetVal,
                                       ErrorResult& aRv) {
  MOZ_ASSERT(dom::IsCurrentThreadRunningWorker());

  dom::WorkerPrivate* workerPrivate = dom::GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  RefPtr<RequestWorkerRunnable> runnable =
      new RequestWorkerRunnable(workerPrivate, this);

  if (mStackHolder.isSome()) {
    runnable->SetSerializedCallerStack(mStackHolder.extract());
  }

  RefPtr<dom::Promise> domPromise;

  IgnoredErrorResult rv;

  switch (mRequestType) {
    case APIRequestType::CALL_FUNCTION_ASYNC:
      domPromise = dom::Promise::Create(aGlobal, rv);
      if (NS_WARN_IF(rv.Failed())) {
        ThrowUnexpectedError(aCx, aRv);
        return;
      }

      runnable->Init(aGlobal, aCx, aArgs, domPromise, rv);
      break;

    case APIRequestType::ADD_LISTENER:
      [[fallthrough]];
    case APIRequestType::REMOVE_LISTENER:
      runnable->Init(aGlobal, aCx, aArgs, aListener, aRv);
      break;

    default:
      runnable->Init(aGlobal, aCx, aArgs, rv);
  }

  if (NS_WARN_IF(rv.Failed())) {
    ThrowUnexpectedError(aCx, aRv);
    return;
  }

  runnable->Dispatch(dom::WorkerStatus::Canceling, rv);
  if (NS_WARN_IF(rv.Failed())) {
    ThrowUnexpectedError(aCx, aRv);
    return;
  }

  auto resultType = runnable->GetResultType();
  if (resultType.isNothing()) {
    if (NS_WARN_IF(ExtensionAPIRequest::ShouldHaveResult(mRequestType))) {
      ThrowUnexpectedError(aCx, aRv);
    }
    return;
  }

  // Read and throw the extension error if needed.
  if (resultType.isSome() && *resultType == APIResultType::EXTENSION_ERROR) {
    JS::Rooted<JS::Value> ignoredResultValue(aCx);
    runnable->ReadResult(aCx, &ignoredResultValue, aRv);
    // When the result type is an error aRv is expected to be
    // failed, if it is not throw the generic
    // "An unexpected error occurred".
    if (NS_WARN_IF(!aRv.Failed())) {
      ThrowUnexpectedError(aCx, aRv);
    }
    return;
  }

  if (mRequestType == APIRequestType::CALL_FUNCTION_ASYNC) {
    MOZ_ASSERT(domPromise);
    if (NS_WARN_IF(!ToJSValue(aCx, domPromise, aRetVal))) {
      ThrowUnexpectedError(aCx, aRv);
    }
    return;
  }

  JS::Rooted<JS::Value> resultValue(aCx);
  runnable->ReadResult(aCx, &resultValue, rv);
  if (NS_WARN_IF(rv.Failed())) {
    ThrowUnexpectedError(aCx, aRv);
    return;
  }

  aRetVal.set(resultValue);
}

void ExtensionAPIRequestForwarder::Run(nsIGlobalObject* aGlobal, JSContext* aCx,
                                       const dom::Sequence<JS::Value>& aArgs,
                                       JS::MutableHandle<JS::Value> aRetVal,
                                       ErrorResult& aRv) {
  Run(aGlobal, aCx, aArgs, nullptr, aRetVal, aRv);
}

void ExtensionAPIRequestForwarder::Run(nsIGlobalObject* aGlobal, JSContext* aCx,
                                       const dom::Sequence<JS::Value>& aArgs,
                                       ErrorResult& aRv) {
  JS::Rooted<JS::Value> ignoredRetval(aCx);
  Run(aGlobal, aCx, aArgs, nullptr, &ignoredRetval, aRv);
}

void ExtensionAPIRequestForwarder::Run(nsIGlobalObject* aGlobal, JSContext* aCx,
                                       const dom::Sequence<JS::Value>& aArgs,
                                       ExtensionEventListener* aListener,
                                       ErrorResult& aRv) {
  MOZ_ASSERT(aListener);
  JS::Rooted<JS::Value> ignoredRetval(aCx);
  Run(aGlobal, aCx, aArgs, aListener, &ignoredRetval, aRv);
}

void ExtensionAPIRequestForwarder::Run(
    nsIGlobalObject* aGlobal, JSContext* aCx,
    const dom::Sequence<JS::Value>& aArgs,
    const RefPtr<dom::Promise>& aPromiseRetval, ErrorResult& aRv) {
  MOZ_ASSERT(aPromiseRetval);
  JS::Rooted<JS::Value> promisedRetval(aCx);
  Run(aGlobal, aCx, aArgs, &promisedRetval, aRv);
  if (aRv.Failed()) {
    return;
  }
  aPromiseRetval->MaybeResolve(promisedRetval);
}

void ExtensionAPIRequestForwarder::Run(nsIGlobalObject* aGlobal, JSContext* aCx,
                                       JS::MutableHandle<JS::Value> aRetVal,
                                       ErrorResult& aRv) {
  Run(aGlobal, aCx, {}, aRetVal, aRv);
}

namespace {

// Custom PromiseWorkerProxy callback to deserialize error objects
// from ClonedErrorHolder structured clone data.
JSObject* ExtensionAPIRequestStructuredCloneRead(
    JSContext* aCx, JSStructuredCloneReader* aReader,
    const dom::PromiseWorkerProxy* aProxy, uint32_t aTag, uint32_t aData) {
  // Deserialize ClonedErrorHolder that may have been structured cloned
  // as a result of a resolved/rejected promise.
  if (aTag == dom::SCTAG_DOM_CLONED_ERROR_OBJECT) {
    return dom::ClonedErrorHolder::ReadStructuredClone(aCx, aReader, nullptr);
  }

  return nullptr;
}

// Custom PromiseWorkerProxy callback to serialize error objects into
// ClonedErrorHolder structured clone data.
bool ExtensionAPIRequestStructuredCloneWrite(JSContext* aCx,
                                             JSStructuredCloneWriter* aWriter,
                                             dom::PromiseWorkerProxy* aProxy,
                                             JS::Handle<JSObject*> aObj) {
  // Try to serialize the object as a CloneErrorHolder, if it fails then
  // the object wasn't an error.
  IgnoredErrorResult rv;
  RefPtr<dom::ClonedErrorHolder> ceh =
      dom::ClonedErrorHolder::Create(aCx, aObj, rv);
  if (NS_WARN_IF(rv.Failed()) || !ceh) {
    return false;
  }

  return ceh->WriteStructuredClone(aCx, aWriter, nullptr);
}

}  // namespace

RequestWorkerRunnable::RequestWorkerRunnable(
    dom::WorkerPrivate* aWorkerPrivate,
    ExtensionAPIRequestForwarder* aOuterAPIRequest)
    : WorkerMainThreadRunnable(aWorkerPrivate,
                               "ExtensionAPIRequest :: WorkerRunnable"_ns) {
  MOZ_ASSERT(dom::IsCurrentThreadRunningWorker());

  MOZ_ASSERT(aOuterAPIRequest);
  mOuterRequest = aOuterAPIRequest;
}

void RequestWorkerRunnable::Init(nsIGlobalObject* aGlobal, JSContext* aCx,
                                 const dom::Sequence<JS::Value>& aArgs,
                                 ExtensionEventListener* aListener,
                                 ErrorResult& aRv) {
  MOZ_ASSERT(dom::IsCurrentThreadRunningWorker());

  mSWDescriptorId = mWorkerPrivate->ServiceWorkerID();

  auto* workerScope = mWorkerPrivate->GlobalScope();
  if (NS_WARN_IF(!workerScope)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  mClientInfo = workerScope->GetClientInfo();
  if (mClientInfo.isNothing()) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  IgnoredErrorResult rv;
  SerializeArgs(aCx, aArgs, rv);
  if (NS_WARN_IF(rv.Failed())) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  if (!mStackHolder.isSome()) {
    SerializeCallerStack(aCx);
  }

  mEventListener = aListener;
}

void RequestWorkerRunnable::Init(nsIGlobalObject* aGlobal, JSContext* aCx,
                                 const dom::Sequence<JS::Value>& aArgs,
                                 const RefPtr<dom::Promise>& aPromiseRetval,
                                 ErrorResult& aRv) {
  // Custom callbacks needed to make the PromiseWorkerProxy instance to
  // be able to write and read errors using CloneErrorHolder.
  static const dom::PromiseWorkerProxy::
      PromiseWorkerProxyStructuredCloneCallbacks
          kExtensionAPIRequestStructuredCloneCallbacks = {
              ExtensionAPIRequestStructuredCloneRead,
              ExtensionAPIRequestStructuredCloneWrite,
          };

  Init(aGlobal, aCx, aArgs, /* aListener */ nullptr, aRv);
  if (aRv.Failed()) {
    return;
  }

  RefPtr<dom::PromiseWorkerProxy> promiseProxy =
      dom::PromiseWorkerProxy::Create(
          mWorkerPrivate, aPromiseRetval,
          &kExtensionAPIRequestStructuredCloneCallbacks);
  if (!promiseProxy) {
    aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
    return;
  }
  mPromiseProxy = promiseProxy.forget();
}

void RequestWorkerRunnable::SetSerializedCallerStack(
    UniquePtr<dom::SerializedStackHolder> aCallerStack) {
  MOZ_ASSERT(dom::IsCurrentThreadRunningWorker());
  MOZ_ASSERT(mStackHolder.isNothing());
  mStackHolder = Some(std::move(aCallerStack));
}

void RequestWorkerRunnable::SerializeCallerStack(JSContext* aCx) {
  MOZ_ASSERT(dom::IsCurrentThreadRunningWorker());
  MOZ_ASSERT(mStackHolder.isNothing());
  mStackHolder = Some(dom::GetCurrentStack(aCx));
}

void RequestWorkerRunnable::DeserializeCallerStack(
    JSContext* aCx, JS::MutableHandle<JS::Value> aRetval) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mStackHolder.isSome()) {
    JS::Rooted<JSObject*> savedFrame(aCx, mStackHolder->get()->ReadStack(aCx));
    MOZ_ASSERT(savedFrame);
    aRetval.set(JS::ObjectValue(*savedFrame));
    mStackHolder = Nothing();
  }
}

void RequestWorkerRunnable::SerializeArgs(JSContext* aCx,
                                          const dom::Sequence<JS::Value>& aArgs,
                                          ErrorResult& aRv) {
  MOZ_ASSERT(dom::IsCurrentThreadRunningWorker());
  MOZ_ASSERT(!mArgsHolder);

  JS::Rooted<JS::Value> jsval(aCx);
  if (NS_WARN_IF(!ToJSValue(aCx, aArgs, &jsval))) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  mArgsHolder = Some(MakeUnique<dom::StructuredCloneHolder>(
      dom::StructuredCloneHolder::CloningSupported,
      dom::StructuredCloneHolder::TransferringNotSupported,
      JS::StructuredCloneScope::SameProcess));
  mArgsHolder->get()->Write(aCx, jsval, aRv);
}

nsresult RequestWorkerRunnable::DeserializeArgs(
    JSContext* aCx, JS::MutableHandle<JS::Value> aArgs) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mArgsHolder.isSome() && mArgsHolder->get()->HasData()) {
    IgnoredErrorResult rv;

    JS::Rooted<JS::Value> jsvalue(aCx);
    mArgsHolder->get()->Read(xpc::CurrentNativeGlobal(aCx), aCx, &jsvalue, rv);
    if (NS_WARN_IF(rv.Failed())) {
      return NS_ERROR_UNEXPECTED;
    }

    aArgs.set(jsvalue);
  }

  return NS_OK;
}

bool RequestWorkerRunnable::MainThreadRun() {
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<mozIExtensionAPIRequestHandler> handler =
      &ExtensionAPIRequestForwarder::APIRequestHandler();
  nsCOMPtr<nsIXPConnectWrappedJS> wrapped = do_QueryInterface(handler);
  dom::AutoJSAPI jsapi;
  if (!jsapi.Init(wrapped->GetJSObjectGlobal())) {
    return false;
  }

  auto* cx = jsapi.cx();
  JS::Rooted<JS::Value> retval(cx);
  return HandleAPIRequest(cx, &retval);
}

already_AddRefed<ExtensionAPIRequest> RequestWorkerRunnable::CreateAPIRequest(
    JSContext* aCx) {
  JS::Rooted<JS::Value> callArgs(aCx);
  JS::Rooted<JS::Value> callerStackValue(aCx);

  DeserializeArgs(aCx, &callArgs);
  DeserializeCallerStack(aCx, &callerStackValue);

  RefPtr<ExtensionAPIRequest> request = new ExtensionAPIRequest(
      mOuterRequest->GetRequestType(), *mOuterRequest->GetRequestTarget());
  request->Init(mClientInfo, mSWDescriptorId, callArgs, callerStackValue);

  if (mEventListener) {
    request->SetEventListener(mEventListener.forget());
  }

  return request.forget();
}

already_AddRefed<WebExtensionPolicy>
RequestWorkerRunnable::GetWebExtensionPolicy() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mWorkerPrivate);
  auto* baseURI = mWorkerPrivate->GetBaseURI();
  RefPtr<WebExtensionPolicy> policy =
      ExtensionPolicyService::GetSingleton().GetByURL(baseURI);
  return policy.forget();
}

bool RequestWorkerRunnable::HandleAPIRequest(
    JSContext* aCx, JS::MutableHandle<JS::Value> aRetval) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<WebExtensionPolicy> policy = GetWebExtensionPolicy();
  if (NS_WARN_IF(!policy || !policy->Active())) {
    // Fails if no extension policy object has been found, or if the
    // extension is not active.
    return false;
  }

  nsresult rv;

  RefPtr<ExtensionAPIRequest> request = CreateAPIRequest(aCx);

  nsCOMPtr<mozIExtensionAPIRequestHandler> handler =
      &ExtensionAPIRequestForwarder::APIRequestHandler();
  RefPtr<mozIExtensionAPIRequestResult> apiResult;
  rv = handler->HandleAPIRequest(policy, request, getter_AddRefs(apiResult));

  if (NS_FAILED(rv)) {
    return false;
  }

  // A missing apiResult is expected for some request types
  // (e.g. CALL_FUNCTION_NO_RETURN/ADD_LISTENER/REMOVE_LISTENER).
  // If the apiResult is missing for a request type that expects
  // to have one, consider the request as failed with an unknown error.
  if (!apiResult) {
    return !request->ShouldHaveResult();
  }

  mozIExtensionAPIRequestResult::ResultType resultType;
  apiResult->GetType(&resultType);
  apiResult->GetValue(aRetval);

  mResultType = Some(resultType);

  bool isExtensionError =
      resultType == mozIExtensionAPIRequestResult::ResultType::EXTENSION_ERROR;
  bool okSerializedError = false;

  if (aRetval.isObject()) {
    // Try to serialize the result as an ClonedErrorHolder
    // (because all API requests could receive one for EXTENSION_ERROR
    // result types, and some also as a RETURN_VALUE result, e.g.
    // runtime.lastError).
    JS::Rooted<JSObject*> errObj(aCx, &aRetval.toObject());
    IgnoredErrorResult rv;
    RefPtr<dom::ClonedErrorHolder> ceh =
        dom::ClonedErrorHolder::Create(aCx, errObj, rv);
    if (!rv.Failed() && ceh) {
      JS::Rooted<JSObject*> obj(aCx);
      // Note: `ToJSValue` cannot be used because ClonedErrorHolder isn't
      // wrapper cached.
      okSerializedError = ceh->WrapObject(aCx, nullptr, &obj);
      aRetval.setObject(*obj);
    } else {
      okSerializedError = false;
    }
  }

  if (isExtensionError && !okSerializedError) {
    NS_WARNING("Failed to wrap ClonedErrorHolder");
    MOZ_DIAGNOSTIC_ASSERT(false, "Failed to wrap ClonedErrorHolder");
    return false;
  }

  if (isExtensionError && !aRetval.isObject()) {
    NS_WARNING("Unexpected non-object error");
    return false;
  }

  switch (resultType) {
    case mozIExtensionAPIRequestResult::ResultType::RETURN_VALUE:
      return ProcessHandlerResult(aCx, aRetval);
    case mozIExtensionAPIRequestResult::ResultType::EXTENSION_ERROR:
      if (!aRetval.isObject()) {
        return false;
      }
      return ProcessHandlerResult(aCx, aRetval);
  }

  MOZ_DIAGNOSTIC_ASSERT(false, "Unexpected API request ResultType");
  return false;
}

bool RequestWorkerRunnable::ProcessHandlerResult(
    JSContext* aCx, JS::MutableHandle<JS::Value> aRetval) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mOuterRequest->GetRequestType() == APIRequestType::CALL_FUNCTION_ASYNC) {
    if (NS_WARN_IF(mResultType.isNothing())) {
      return false;
    }

    if (*mResultType == APIResultType::RETURN_VALUE) {
      // For an Async API method we expect a promise object to be set
      // as the value to return, if it is not we return earlier here
      // (and then throw a generic unexpected error to the caller).
      if (NS_WARN_IF(!aRetval.isObject())) {
        return false;
      }
      JS::Rooted<JSObject*> obj(aCx, &aRetval.toObject());
      if (NS_WARN_IF(!JS::IsPromiseObject(obj))) {
        return false;
      }

      ErrorResult rv;
      nsIGlobalObject* glob = xpc::CurrentNativeGlobal(aCx);
      RefPtr<dom::Promise> retPromise =
          dom::Promise::Resolve(glob, aCx, aRetval, rv);
      if (rv.Failed()) {
        return false;
      }
      retPromise->AppendNativeHandler(mPromiseProxy);
      return true;
    }
  }

  switch (*mResultType) {
    case APIResultType::RETURN_VALUE:
      [[fallthrough]];
    case APIResultType::EXTENSION_ERROR: {
      // In all other case we expect the result to be:
      // - a structured clonable result
      // - an extension error (e.g. due to the API call params validation
      // errors),
      //   previously converted into a CloneErrorHolder
      IgnoredErrorResult rv;
      mResultHolder = Some(MakeUnique<dom::StructuredCloneHolder>(
          dom::StructuredCloneHolder::CloningSupported,
          dom::StructuredCloneHolder::TransferringNotSupported,
          JS::StructuredCloneScope::SameProcess));
      mResultHolder->get()->Write(aCx, aRetval, rv);
      return !rv.Failed();
    }
  }

  MOZ_DIAGNOSTIC_ASSERT(false, "Unexpected API request ResultType");
  return false;
}

void RequestWorkerRunnable::ReadResult(JSContext* aCx,
                                       JS::MutableHandle<JS::Value> aResult,
                                       ErrorResult& aRv) {
  MOZ_ASSERT(mWorkerPrivate->IsOnCurrentThread());
  if (mResultHolder.isNothing() || !mResultHolder->get()->HasData()) {
    return;
  }

  if (NS_WARN_IF(mResultType.isNothing())) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  switch (*mResultType) {
    case mozIExtensionAPIRequestResult::ResultType::RETURN_VALUE:
      mResultHolder->get()->Read(xpc::CurrentNativeGlobal(aCx), aCx, aResult,
                                 aRv);
      return;
    case mozIExtensionAPIRequestResult::ResultType::EXTENSION_ERROR:
      JS::Rooted<JS::Value> exn(aCx);
      IgnoredErrorResult rv;
      mResultHolder->get()->Read(xpc::CurrentNativeGlobal(aCx), aCx, &exn, rv);
      if (rv.Failed()) {
        NS_WARNING("Failed to deserialize extension error");
        ExtensionAPIBase::ThrowUnexpectedError(aCx, aRv);
        return;
      }

      aRv.MightThrowJSException();
      aRv.ThrowJSException(aCx, exn);
      return;
  }

  MOZ_DIAGNOSTIC_ASSERT(false, "Unexpected API request ResultType");
  aRv.Throw(NS_ERROR_UNEXPECTED);
}

// RequestInitWorkerContextRunnable

RequestInitWorkerRunnable::RequestInitWorkerRunnable(
    dom::WorkerPrivate* aWorkerPrivate, Maybe<dom::ClientInfo>& aSWClientInfo)
    : WorkerMainThreadRunnable(aWorkerPrivate,
                               "extensions::RequestInitWorkerRunnable"_ns) {
  MOZ_ASSERT(dom::IsCurrentThreadRunningWorker());
  MOZ_ASSERT(aSWClientInfo.isSome());
  mClientInfo = aSWClientInfo;
}

bool RequestInitWorkerRunnable::MainThreadRun() {
  MOZ_ASSERT(NS_IsMainThread());

  auto* baseURI = mWorkerPrivate->GetBaseURI();
  RefPtr<WebExtensionPolicy> policy =
      ExtensionPolicyService::GetSingleton().GetByURL(baseURI);

  RefPtr<ExtensionServiceWorkerInfo> swInfo = new ExtensionServiceWorkerInfo(
      *mClientInfo, mWorkerPrivate->ServiceWorkerID());

  nsCOMPtr<mozIExtensionAPIRequestHandler> handler =
      &ExtensionAPIRequestForwarder::APIRequestHandler();
  MOZ_ASSERT(handler);

  if (NS_FAILED(handler->InitExtensionWorker(policy, swInfo))) {
    NS_WARNING("nsIExtensionAPIRequestHandler.initExtensionWorker call failed");
  }

  return true;
}

// NotifyWorkerLoadedRunnable

nsresult NotifyWorkerLoadedRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<WebExtensionPolicy> policy =
      ExtensionPolicyService::GetSingleton().GetByURL(mSWBaseURI.get());

  nsCOMPtr<mozIExtensionAPIRequestHandler> handler =
      &ExtensionAPIRequestForwarder::APIRequestHandler();
  MOZ_ASSERT(handler);

  if (NS_FAILED(handler->OnExtensionWorkerLoaded(policy, mSWDescriptorId))) {
    NS_WARNING(
        "nsIExtensionAPIRequestHandler.onExtensionWorkerLoaded call failed");
  }

  return NS_OK;
}

// NotifyWorkerDestroyedRunnable

nsresult NotifyWorkerDestroyedRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<WebExtensionPolicy> policy =
      ExtensionPolicyService::GetSingleton().GetByURL(mSWBaseURI.get());

  nsCOMPtr<mozIExtensionAPIRequestHandler> handler =
      &ExtensionAPIRequestForwarder::APIRequestHandler();
  MOZ_ASSERT(handler);

  if (NS_FAILED(handler->OnExtensionWorkerDestroyed(policy, mSWDescriptorId))) {
    NS_WARNING(
        "nsIExtensionAPIRequestHandler.onExtensionWorkerDestroyed call failed");
  }

  return NS_OK;
}

}  // namespace extensions
}  // namespace mozilla
