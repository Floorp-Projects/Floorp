/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionEventListener.h"

#include "mozilla/dom/FunctionBinding.h"
#include "nsJSPrincipals.h"   // nsJSPrincipals::AutoSetActiveWorkerPrincipal
#include "nsThreadManager.h"  // NS_IsMainThread

namespace mozilla {
namespace extensions {

// ExtensionEventListener

NS_IMPL_ISUPPORTS(ExtensionEventListener, mozIExtensionEventListener)

// static
already_AddRefed<ExtensionEventListener> ExtensionEventListener::Create(
    nsIGlobalObject* aGlobal, dom::Function* aCallback,
    CleanupCallback&& aCleanupCallback, ErrorResult& aRv) {
  MOZ_ASSERT(dom::IsCurrentThreadRunningWorker());
  RefPtr<ExtensionEventListener> extCb =
      new ExtensionEventListener(aGlobal, aCallback);

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
    const nsTArray<JS::Value>& aArgs, JSContext* aCx,
    dom::Promise** aPromiseResult) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aPromiseResult);

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

  MutexAutoLock lock(mMutex);

  if (NS_WARN_IF(!mWorkerRef)) {
    return NS_ERROR_ABORT;
  }

  UniquePtr<dom::StructuredCloneHolder> argsHolder =
      SerializeCallArguments(aArgs, aCx, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  RefPtr<ExtensionListenerCallWorkerRunnable> runnable =
      new ExtensionListenerCallWorkerRunnable(this, std::move(argsHolder),
                                              retPromise);
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
    retPromise->MaybeReject(std::move(erv));
  } else {
    retPromise->MaybeResolve(retval);
  }

  ExtensionListenerCallPromiseResultHandler::Create(
      retPromise, this, new dom::ThreadSafeWorkerRef(workerRef));
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

  JS::RootedValue retval(aCx, aValue);

  if (retval.isObject()) {
    // Try to serialize the result as an ClonedErrorHolder,
    // in case the value is an Error object.
    IgnoredErrorResult rv;
    JS::Rooted<JSObject*> errObj(aCx, &retval.toObject());
    RefPtr<dom::ClonedErrorHolder> ceh =
        dom::ClonedErrorHolder::Create(aCx, errObj, rv);
    if (!rv.Failed() && ceh) {
      JS::RootedObject obj(aCx);
      // Note: `ToJSValue` cannot be used because ClonedErrorHolder isn't
      // wrapper cached.
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

    {
      // Set the active worker principal while reading the result,
      // needed to be sure to be able to successfully deserialize the
      // SavedFrame part of a ClonedErrorHolder (in case that was the
      // result stored in the StructuredCloneHolder).
      Maybe<nsJSPrincipals::AutoSetActiveWorkerPrincipal> set;
      if (workerRef) {
        set.emplace(workerRef->Private()->GetPrincipal());
      }

      resHolder->Read(global, cx, &jsvalue, rv);
    }

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
    JSContext* aCx, JS::Handle<JS::Value> aValue) {
  WorkerRunCallback(aCx, aValue, PromiseCallbackType::Resolve);
}

void ExtensionListenerCallPromiseResultHandler::RejectedCallback(
    JSContext* aCx, JS::Handle<JS::Value> aValue) {
  WorkerRunCallback(aCx, aValue, PromiseCallbackType::Reject);
}

}  // namespace extensions
}  // namespace mozilla
