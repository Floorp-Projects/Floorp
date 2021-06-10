/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionEventListener.h"

#include "mozilla/dom/FunctionBinding.h"
#include "nsThreadManager.h"  // NS_IsMainThread

namespace mozilla {
namespace extensions {

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
                                               ErrorResult& aRv) {
  dom::AutoEntryScript aes(xpc::PrivilegedJunkScope(),
                           "ExtensionEventListener :: CallListener");
  JSContext* cx = aes.cx();
  JS::Rooted<JS::Value> jsval(cx);

  if (NS_WARN_IF(!dom::ToJSValue(cx, aArgs, &jsval))) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  UniquePtr<dom::StructuredCloneHolder> argsHolder =
      MakeUnique<dom::StructuredCloneHolder>(
          dom::StructuredCloneHolder::CloningSupported,
          dom::StructuredCloneHolder::TransferringNotSupported,
          JS::StructuredCloneScope::SameProcess);

  argsHolder->Write(cx, jsval, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return argsHolder;
}

NS_IMETHODIMP ExtensionEventListener::CallListener(
    const nsTArray<JS::Value>& aArgs) {
  MOZ_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mMutex);

  if (NS_WARN_IF(!mWorkerRef)) {
    return NS_ERROR_ABORT;
  }

  ErrorResult rv;
  UniquePtr<dom::StructuredCloneHolder> argsHolder =
      SerializeCallArguments(aArgs, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  RefPtr<ExtensionListenerCallWorkerRunnable> runnable =
      new ExtensionListenerCallWorkerRunnable(this, std::move(argsHolder));
  runnable->Dispatch();

  return NS_OK;
}

dom::WorkerPrivate* ExtensionEventListener::GetWorkerPrivate() const {
  MOZ_ASSERT(mWorkerRef);
  return mWorkerRef->Private();
}

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

  ErrorResult rv;
  dom::Sequence<JS::Value> argsSequence;
  dom::SequenceRooter<JS::Value> arguments(aCx, &argsSequence);

  DeserializeCallArguments(aCx, argsSequence, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return true;
  }

  JS::Rooted<JS::Value> ignoredRetval(aCx);
  MOZ_KnownLive(fn)->Call(MOZ_KnownLive(global), argsSequence, &ignoredRetval,
                          rv);
  // Return false if calling the callback did throw an uncatchable exception.
  return !rv.IsUncatchableException();
}

}  // namespace extensions
}  // namespace mozilla
