/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionEventListener.h"
#include "ExtensionAPIRequestForwarder.h"
#include "ExtensionPort.h"

#include "mozilla/dom/ClonedErrorHolder.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Promise-inl.h"
#include "nsThreadManager.h"  // NS_IsMainThread

namespace mozilla::extensions {

namespace {

class SendResponseCallback final : public nsISupports {
 public:
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(SendResponseCallback)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  static RefPtr<SendResponseCallback> Create(
      nsIGlobalObject* aGlobalObject, const RefPtr<dom::Promise>& aPromise,
      JS::Handle<JS::Value> aValue, ErrorResult& aRv) {
    MOZ_ASSERT(dom::IsCurrentThreadRunningWorker());

    RefPtr<SendResponseCallback> responseCallback =
        new SendResponseCallback(aPromise, aValue);

    // Create a promise monitor that invalidates the sendResponse
    // callback if the promise has been already resolved or rejected.
    aPromise->AddCallbacksWithCycleCollectedArgs(
        [](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv,
           SendResponseCallback* aCallback) { aCallback->Cleanup(); },
        [](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv,
           SendResponseCallback* aCallback) { aCallback->Cleanup(); },
        responseCallback);

    auto cleanupCb = [responseCallback]() { responseCallback->Cleanup(); };

    // Create a StrongWorkerRef to the worker thread, the cleanup callback
    // associated to the StrongWorkerRef will release the reference and resolve
    // the promise returned to the ExtensionEventListener caller with undefined
    // if the worker global is being destroyed.
    auto* workerPrivate = dom::GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    workerPrivate->AssertIsOnWorkerThread();

    RefPtr<dom::StrongWorkerRef> workerRef = dom::StrongWorkerRef::Create(
        workerPrivate, "SendResponseCallback", cleanupCb);
    if (NS_WARN_IF(!workerRef)) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    responseCallback->mWorkerRef = workerRef;

    return responseCallback;
  }

  SendResponseCallback(const RefPtr<dom::Promise>& aPromise,
                       JS::Handle<JS::Value> aValue)
      : mPromise(aPromise), mValue(aValue) {
    MOZ_ASSERT(mPromise);
    mozilla::HoldJSObjects(this);
  }

  void Cleanup(bool aIsDestroying = false) {
    // Return earlier if the instance has already been cleaned up.
    if (!mPromise) {
      return;
    }

    NS_WARNING("SendResponseCallback::Cleanup");

    mPromise->MaybeResolveWithUndefined();
    mPromise = nullptr;

    // Skipped if called from the destructor.
    if (!aIsDestroying && mValue.isObject()) {
      // Release the reference to the SendResponseCallback.
      js::SetFunctionNativeReserved(&mValue.toObject(),
                                    SLOT_SEND_RESPONSE_CALLBACK_INSTANCE,
                                    JS::PrivateValue(nullptr));
    }

    if (mWorkerRef) {
      mWorkerRef = nullptr;
    }
  }

  static bool Call(JSContext* aCx, unsigned aArgc, JS::Value* aVp) {
    JS::CallArgs args = CallArgsFromVp(aArgc, aVp);
    JS::Rooted<JSObject*> callee(aCx, &args.callee());

    JS::Value v = js::GetFunctionNativeReserved(
        callee, SLOT_SEND_RESPONSE_CALLBACK_INSTANCE);

    SendResponseCallback* sendResponse =
        reinterpret_cast<SendResponseCallback*>(v.toPrivate());
    if (!sendResponse || !sendResponse->mPromise ||
        !sendResponse->mPromise->PromiseObj()) {
      NS_WARNING("SendResponseCallback called after being invalidated");
      return true;
    }

    sendResponse->mPromise->MaybeResolve(args.get(0));
    sendResponse->Cleanup();

    return true;
  }

 private:
  ~SendResponseCallback() {
    mozilla::DropJSObjects(this);
    this->Cleanup(true);
  };

  RefPtr<dom::Promise> mPromise;
  JS::Heap<JS::Value> mValue;
  RefPtr<dom::StrongWorkerRef> mWorkerRef;
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SendResponseCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(SendResponseCallback)

NS_IMPL_CYCLE_COLLECTING_ADDREF(SendResponseCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SendResponseCallback)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(SendResponseCallback)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromise)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(SendResponseCallback)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mValue)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(SendResponseCallback)
  tmp->mValue.setUndefined();
  // Resolve the promise with undefined (as "unhandled") before unlinking it.
  if (tmp->mPromise) {
    tmp->mPromise->MaybeResolveWithUndefined();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPromise);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

}  // anonymous namespace

// ExtensionEventListener

NS_IMPL_ISUPPORTS(ExtensionEventListener, mozIExtensionEventListener)

// static
already_AddRefed<ExtensionEventListener> ExtensionEventListener::Create(
    nsIGlobalObject* aGlobal, ExtensionBrowser* aExtensionBrowser,
    dom::Function* aCallback, CleanupCallback&& aCleanupCallback,
    ErrorResult& aRv) {
  MOZ_ASSERT(dom::IsCurrentThreadRunningWorker());
  RefPtr<ExtensionEventListener> extCb =
      new ExtensionEventListener(aGlobal, aExtensionBrowser, aCallback);

  auto* workerPrivate = dom::GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();
  RefPtr<dom::StrongWorkerRef> workerRef = dom::StrongWorkerRef::Create(
      workerPrivate, "ExtensionEventListener", std::move(aCleanupCallback));
  if (!workerRef) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  extCb->mWorkerRef = new dom::ThreadSafeWorkerRef(workerRef);

  return extCb.forget();
}

// static
UniquePtr<dom::StructuredCloneHolder>
ExtensionEventListener::SerializeCallArguments(const nsTArray<JS::Value>& aArgs,
                                               JSContext* aCx,
                                               ErrorResult& aRv) {
  JS::Rooted<JS::Value> jsval(aCx);
  if (NS_WARN_IF(!dom::ToJSValue(aCx, aArgs, &jsval))) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  UniquePtr<dom::StructuredCloneHolder> argsHolder =
      MakeUnique<dom::StructuredCloneHolder>(
          dom::StructuredCloneHolder::CloningSupported,
          dom::StructuredCloneHolder::TransferringNotSupported,
          JS::StructuredCloneScope::SameProcess);

  argsHolder->Write(aCx, jsval, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return argsHolder;
}

NS_IMETHODIMP ExtensionEventListener::CallListener(
    const nsTArray<JS::Value>& aArgs, ListenerCallOptions* aCallOptions,
    JSContext* aCx, dom::Promise** aPromiseResult) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aPromiseResult);

  // Process and validate call options.
  APIObjectType apiObjectType = APIObjectType::NONE;
  JS::Rooted<JS::Value> apiObjectDescriptor(aCx);
  if (aCallOptions) {
    aCallOptions->GetApiObjectType(&apiObjectType);
    aCallOptions->GetApiObjectDescriptor(&apiObjectDescriptor);

    // Explicitly check that the APIObjectType is one of expected ones,
    // raise to the caller an explicit error if it is not.
    //
    // This is using a switch to also get a warning if a new value is added to
    // the APIObjectType enum and it is not yet handled.
    switch (apiObjectType) {
      case APIObjectType::NONE:
        if (NS_WARN_IF(!apiObjectDescriptor.isNullOrUndefined())) {
          JS_ReportErrorASCII(
              aCx,
              "Unexpected non-null apiObjectDescriptor on apiObjectType=NONE");
          return NS_ERROR_UNEXPECTED;
        }
        break;
      case APIObjectType::RUNTIME_PORT:
        if (NS_WARN_IF(apiObjectDescriptor.isNullOrUndefined())) {
          JS_ReportErrorASCII(aCx,
                              "Unexpected null apiObjectDescriptor on "
                              "apiObjectType=RUNTIME_PORT");
          return NS_ERROR_UNEXPECTED;
        }
        break;
      default:
        MOZ_CRASH("Unexpected APIObjectType");
        return NS_ERROR_UNEXPECTED;
    }
  }

  // Create promise to be returned.
  IgnoredErrorResult rv;
  RefPtr<dom::Promise> retPromise;

  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!global)) {
    return NS_ERROR_FAILURE;
  }
  retPromise = dom::Promise::Create(global, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  // Convert args into a non-const sequence.
  dom::Sequence<JS::Value> args;
  if (!args.AppendElements(aArgs, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Execute the listener call.

  MutexAutoLock lock(mMutex);

  if (NS_WARN_IF(!mWorkerRef)) {
    return NS_ERROR_ABORT;
  }

  if (apiObjectType != APIObjectType::NONE) {
    bool prependArgument = false;
    aCallOptions->GetApiObjectPrepended(&prependArgument);
    // Prepend or append the apiObjectDescriptor data to the call arguments,
    // the worker runnable will convert that into an API object
    // instance on the worker thread.
    if (prependArgument) {
      if (!args.InsertElementAt(0, std::move(apiObjectDescriptor), fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    } else {
      if (!args.AppendElement(std::move(apiObjectDescriptor), fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  UniquePtr<dom::StructuredCloneHolder> argsHolder =
      SerializeCallArguments(args, aCx, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  RefPtr<ExtensionListenerCallWorkerRunnable> runnable =
      new ExtensionListenerCallWorkerRunnable(this, std::move(argsHolder),
                                              aCallOptions, retPromise);
  runnable->Dispatch();
  retPromise.forget(aPromiseResult);

  return NS_OK;
}

dom::WorkerPrivate* ExtensionEventListener::GetWorkerPrivate() const {
  MOZ_ASSERT(mWorkerRef);
  return mWorkerRef->Private();
}

// ExtensionListenerCallWorkerRunnable

void ExtensionListenerCallWorkerRunnable::DeserializeCallArguments(
    JSContext* aCx, dom::Sequence<JS::Value>& aArgs, ErrorResult& aRv) {
  JS::Rooted<JS::Value> jsvalue(aCx);

  mArgsHolder->Read(xpc::CurrentNativeGlobal(aCx), aCx, &jsvalue, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsresult rv2 =
      ExtensionAPIRequestForwarder::JSArrayToSequence(aCx, jsvalue, aArgs);
  if (NS_FAILED(rv2)) {
    aRv.Throw(rv2);
  }
}

bool ExtensionListenerCallWorkerRunnable::WorkerRun(
    JSContext* aCx, dom::WorkerPrivate* aWorkerPrivate) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(aWorkerPrivate == mWorkerPrivate);
  auto global = mListener->GetGlobalObject();
  if (NS_WARN_IF(!global)) {
    return true;
  }

  RefPtr<ExtensionBrowser> extensionBrowser = mListener->GetExtensionBrowser();
  if (NS_WARN_IF(!extensionBrowser)) {
    return true;
  }

  auto fn = mListener->GetCallback();
  if (NS_WARN_IF(!fn)) {
    return true;
  }

  IgnoredErrorResult rv;
  dom::Sequence<JS::Value> argsSequence;
  dom::SequenceRooter<JS::Value> arguments(aCx, &argsSequence);

  DeserializeCallArguments(aCx, argsSequence, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return true;
  }

  RefPtr<dom::Promise> retPromise;
  RefPtr<dom::StrongWorkerRef> workerRef;

  retPromise = dom::Promise::Create(global, rv);
  if (retPromise) {
    workerRef = dom::StrongWorkerRef::Create(
        aWorkerPrivate, "ExtensionListenerCallWorkerRunnable", []() {});
  }

  if (NS_WARN_IF(rv.Failed() || !workerRef)) {
    auto rejectMainThreadPromise =
        [error = rv.Failed() ? rv.StealNSResult() : NS_ERROR_UNEXPECTED,
         promiseResult = std::move(mPromiseResult)]() {
          // TODO(rpl): this seems to be currently rejecting an error object
          // without a stack trace, its a corner case but we may look into
          // improve this error.
          promiseResult->MaybeReject(error);
        };

    nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableFunction(__func__, std::move(rejectMainThreadPromise));
    NS_DispatchToMainThread(runnable);
    JS_ClearPendingException(aCx);
    return true;
  }

  ExtensionListenerCallPromiseResultHandler::Create(
      retPromise, this, new dom::ThreadSafeWorkerRef(workerRef));

  // Translate the first parameter into the API object type (e.g. an
  // ExtensionPort), the content of the original argument value is expected to
  // be a dictionary that is valid as an internal descriptor for that API object
  // type.
  if (mAPIObjectType != APIObjectType::NONE) {
    IgnoredErrorResult rv;

    // The api object descriptor is expected to have been prepended to the
    // other arguments, assert here that the argsSequence does contain at least
    // one element.
    MOZ_ASSERT(!argsSequence.IsEmpty());

    uint32_t apiObjectIdx = mAPIObjectPrepended ? 0 : argsSequence.Length() - 1;
    JS::Rooted<JS::Value> apiObjectDescriptor(
        aCx, argsSequence.ElementAt(apiObjectIdx));
    JS::Rooted<JS::Value> apiObjectValue(aCx);

    // We only expect the object type to be RUNTIME_PORT at the moment,
    // until we will need to expect it to support other object types that
    // some specific API may need.
    MOZ_ASSERT(mAPIObjectType == APIObjectType::RUNTIME_PORT);
    RefPtr<ExtensionPort> port =
        extensionBrowser->GetPort(apiObjectDescriptor, rv);
    if (NS_WARN_IF(rv.Failed())) {
      retPromise->MaybeReject(rv.StealNSResult());
      return true;
    }

    if (NS_WARN_IF(!dom::ToJSValue(aCx, port, &apiObjectValue))) {
      retPromise->MaybeReject(NS_ERROR_UNEXPECTED);
      return true;
    }

    argsSequence.ReplaceElementAt(apiObjectIdx, apiObjectValue);
  }

  // Create callback argument and append it to the call arguments.
  JS::Rooted<JSObject*> sendResponseObj(aCx);

  switch (mCallbackArgType) {
    case CallbackType::CALLBACK_NONE:
      break;
    case CallbackType::CALLBACK_SEND_RESPONSE: {
      JS::Rooted<JSFunction*> sendResponseFn(
          aCx, js::NewFunctionWithReserved(aCx, SendResponseCallback::Call,
                                           /* nargs */ 1, 0, "sendResponse"));
      sendResponseObj = JS_GetFunctionObject(sendResponseFn);
      JS::Rooted<JS::Value> sendResponseValue(
          aCx, JS::ObjectValue(*sendResponseObj));

      // Create a SendResponseCallback instance that keeps a reference
      // to the promise to resolve when the static SendReponseCallback::Call
      // is being called.
      // the SendReponseCallback instance from the resolved slot to resolve
      // the promise and invalidated the sendResponse callback (any new call
      // becomes a noop).
      RefPtr<SendResponseCallback> sendResponsePtr =
          SendResponseCallback::Create(global, retPromise, sendResponseValue,
                                       rv);
      if (NS_WARN_IF(rv.Failed())) {
        retPromise->MaybeReject(NS_ERROR_UNEXPECTED);
        return true;
      }

      // Store the SendResponseCallback instance in a private value set on the
      // function object reserved slot, where ehe SendResponseCallback::Call
      // static function will get it back to resolve the related promise
      // and then invalidate the sendResponse callback (any new call
      // becomes a noop).
      js::SetFunctionNativeReserved(sendResponseObj,
                                    SLOT_SEND_RESPONSE_CALLBACK_INSTANCE,
                                    JS::PrivateValue(sendResponsePtr));

      if (NS_WARN_IF(
              !argsSequence.AppendElement(sendResponseValue, fallible))) {
        retPromise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
        return true;
      }

      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected callbackType");
      break;
  }

  // TODO: should `nsAutoMicroTask mt;` be used here?
  dom::AutoEntryScript aes(global, "WebExtensionAPIEvent");
  JS::Rooted<JS::Value> retval(aCx);
  ErrorResult erv;
  erv.MightThrowJSException();
  MOZ_KnownLive(fn)->Call(argsSequence, &retval, erv, "WebExtensionAPIEvent",
                          dom::Function::eRethrowExceptions);

  // Calling the callback may have thrown an exception.
  // TODO: add a ListenerCallOptions to optionally report the exception
  // instead of forwarding it to the caller.
  erv.WouldReportJSException();

  if (erv.Failed()) {
    if (erv.IsUncatchableException()) {
      // TODO: include some more info? (e.g. api path).
      retPromise->MaybeRejectWithTimeoutError(
          "WebExtensions API Event listener threw uncatchable exception");
      return true;
    }

    retPromise->MaybeReject(std::move(erv));
    return true;
  }

  // Custom return value handling logic for events that do pass a
  // sendResponse callback parameter (see expected behavior
  // for the runtime.onMessage sendResponse parameter on MDN:
  // https://mzl.la/3dokpMi):
  //
  // - listener returns Boolean true => the extension listener is
  //   expected to call sendResponse callback parameter asynchronosuly
  // - listener return a Promise object => the promise is the listener
  //   response
  // - listener return any other value => the listener didn't handle the
  //   event and the return value is ignored
  //
  if (mCallbackArgType == CallbackType::CALLBACK_SEND_RESPONSE) {
    if (retval.isBoolean() && retval.isTrue()) {
      // The listener returned `true` and so the promise relate to the
      // listener call will be resolved once the extension will call
      // the sendResponce function passed as a callback argument.
      return true;
    }

    // If the retval isn't true and it is not a Promise object,
    // the listener isn't handling the event, and we resolve the
    // promise with undefined (if the listener didn't reply already
    // by calling sendResponse synchronsouly).
    // undefined (
    if (!ExtensionEventListener::IsPromise(aCx, retval)) {
      // Mark this listener call as cancelled,
      // ExtensionListenerCallPromiseResult will check to know that it should
      // release the main thread promise without resolving it.
      //
      // TODO: double-check if we should also cancel rejecting the promise
      // returned by mozIExtensionEventListener.callListener when the listener
      // call throws (by comparing it with the behavior on the current
      // privileged-based API implementation).
      mIsCallResultCancelled = true;
      retPromise->MaybeResolveWithUndefined();

      // Invalidate the sendResponse function by setting the private
      // value where the SendResponseCallback instance was stored
      // to a nullptr.
      js::SetFunctionNativeReserved(sendResponseObj,
                                    SLOT_SEND_RESPONSE_CALLBACK_INSTANCE,
                                    JS::PrivateValue(nullptr));

      return true;
    }
  }

  retPromise->MaybeResolve(retval);

  return true;
}

// ExtensionListenerCallPromiseResultHandler

NS_IMPL_ISUPPORTS0(ExtensionListenerCallPromiseResultHandler)

// static
void ExtensionListenerCallPromiseResultHandler::Create(
    const RefPtr<dom::Promise>& aPromise,
    const RefPtr<ExtensionListenerCallWorkerRunnable>& aWorkerRunnable,
    dom::ThreadSafeWorkerRef* aWorkerRef) {
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(aWorkerRef);
  MOZ_ASSERT(aWorkerRef->Private()->IsOnCurrentThread());

  RefPtr<ExtensionListenerCallPromiseResultHandler> handler =
      new ExtensionListenerCallPromiseResultHandler(aWorkerRef,
                                                    aWorkerRunnable);
  aPromise->AppendNativeHandler(handler);
}

void ExtensionListenerCallPromiseResultHandler::WorkerRunCallback(
    JSContext* aCx, JS::Handle<JS::Value> aValue,
    PromiseCallbackType aCallbackType) {
  MOZ_ASSERT(mWorkerRef);
  mWorkerRef->Private()->AssertIsOnWorkerThread();

  // The listener call was cancelled (e.g. when a runtime.onMessage listener
  // returned false), release resources associated with this promise handler
  // on the main thread without resolving the promise associated to the
  // extension event listener call.
  if (mWorkerRunnable->IsCallResultCancelled()) {
    auto releaseMainThreadPromise = [runnable = std::move(mWorkerRunnable),
                                     workerRef = std::move(mWorkerRef)]() {};
    nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableFunction(__func__, std::move(releaseMainThreadPromise));
    NS_DispatchToMainThread(runnable);
    return;
  }

  JS::Rooted<JS::Value> retval(aCx, aValue);

  if (retval.isObject()) {
    // Try to serialize the result as an ClonedErrorHolder,
    // in case the value is an Error object.
    IgnoredErrorResult rv;
    JS::Rooted<JSObject*> errObj(aCx, &retval.toObject());
    RefPtr<dom::ClonedErrorHolder> ceh =
        dom::ClonedErrorHolder::Create(aCx, errObj, rv);
    if (!rv.Failed() && ceh) {
      JS::Rooted<JSObject*> obj(aCx);
      // Note: `ToJSValue` cannot be used because ClonedErrorHolder isn't
      // wrapped cached.
      Unused << NS_WARN_IF(!ceh->WrapObject(aCx, nullptr, &obj));
      retval.setObject(*obj);
    }
  }

  UniquePtr<dom::StructuredCloneHolder> resHolder =
      MakeUnique<dom::StructuredCloneHolder>(
          dom::StructuredCloneHolder::CloningSupported,
          dom::StructuredCloneHolder::TransferringNotSupported,
          JS::StructuredCloneScope::SameProcess);

  IgnoredErrorResult erv;
  resHolder->Write(aCx, retval, erv);

  // Failed to serialize the result, dispatch a runnable to reject
  // the promise returned to the caller of the mozIExtensionCallback
  // callWithPromiseResult method.
  if (NS_WARN_IF(erv.Failed())) {
    auto rejectMainThreadPromise = [error = erv.StealNSResult(),
                                    runnable = std::move(mWorkerRunnable),
                                    resHolder = std::move(resHolder)]() {
      RefPtr<dom::Promise> promiseResult = std::move(runnable->mPromiseResult);
      promiseResult->MaybeReject(error);
    };

    nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableFunction(__func__, std::move(rejectMainThreadPromise));
    NS_DispatchToMainThread(runnable);
    JS_ClearPendingException(aCx);
    return;
  }

  auto resolveMainThreadPromise = [callbackType = aCallbackType,
                                   resHolder = std::move(resHolder),
                                   runnable = std::move(mWorkerRunnable),
                                   workerRef = std::move(mWorkerRef)]() {
    RefPtr<dom::Promise> promiseResult = std::move(runnable->mPromiseResult);

    auto* global = promiseResult->GetGlobalObject();
    dom::AutoEntryScript aes(global,
                             "ExtensionListenerCallWorkerRunnable::WorkerRun");
    JSContext* cx = aes.cx();
    JS::Rooted<JS::Value> jsvalue(cx);
    IgnoredErrorResult rv;

    resHolder->Read(global, cx, &jsvalue, rv);

    if (NS_WARN_IF(rv.Failed())) {
      promiseResult->MaybeReject(rv.StealNSResult());
      JS_ClearPendingException(cx);
    } else {
      switch (callbackType) {
        case PromiseCallbackType::Resolve:
          promiseResult->MaybeResolve(jsvalue);
          break;
        case PromiseCallbackType::Reject:
          promiseResult->MaybeReject(jsvalue);
          break;
      }
    }
  };

  nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableFunction(__func__, std::move(resolveMainThreadPromise));
  NS_DispatchToMainThread(runnable);
}

void ExtensionListenerCallPromiseResultHandler::ResolvedCallback(
    JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv) {
  WorkerRunCallback(aCx, aValue, PromiseCallbackType::Resolve);
}

void ExtensionListenerCallPromiseResultHandler::RejectedCallback(
    JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv) {
  WorkerRunCallback(aCx, aValue, PromiseCallbackType::Reject);
}

}  // namespace mozilla::extensions
