/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionEventListener_h
#define mozilla_extensions_ExtensionEventListener_h

#include "mozIExtensionAPIRequestHandling.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerPrivate.h"

class nsIGlobalObject;

namespace mozilla {

namespace dom {
class Function;
}  // namespace dom

namespace extensions {

// A class that represents a callback parameter (passed to addListener /
// removeListener), instances of this class are received by the
// mozIExtensionAPIRequestHandler as a property of the mozIExtensionAPIRequest.
// The mozIExtensionEventListener xpcom interface provides methods that allow
// the mozIExtensionAPIRequestHandler running in the Main Thread to call the
// underlying callback Function on its owning thread.
class ExtensionEventListener final : public mozIExtensionEventListener {
 public:
  NS_DECL_MOZIEXTENSIONEVENTLISTENER
  NS_DECL_THREADSAFE_ISUPPORTS

  using CleanupCallback = std::function<void()>;

  static already_AddRefed<ExtensionEventListener> Create(
      nsIGlobalObject* aGlobal, dom::Function* aCallback,
      CleanupCallback&& aCleanupCallback, ErrorResult& aRv);

  dom::WorkerPrivate* GetWorkerPrivate() const;

  RefPtr<dom::Function> GetCallback() const { return mCallback; }

  nsCOMPtr<nsIGlobalObject> GetGlobalObject() const {
    nsCOMPtr<nsIGlobalObject> global = do_QueryReferent(mGlobal);
    return global;
  }

  void Cleanup() {
    if (mWorkerRef) {
      MutexAutoLock lock(mMutex);

      mWorkerRef->Private()->AssertIsOnWorkerThread();
      mWorkerRef = nullptr;
    }

    mGlobal = nullptr;
    mCallback = nullptr;
  }

 private:
  ExtensionEventListener(nsIGlobalObject* aGlobal, dom::Function* aCallback)
      : mGlobal(do_GetWeakReference(aGlobal)),
        mCallback(aCallback),
        mMutex("ExtensionEventListener::mMutex"){};

  static UniquePtr<dom::StructuredCloneHolder> SerializeCallArguments(
      const nsTArray<JS::Value>& aArgs, ErrorResult& aRv);

  ~ExtensionEventListener() { Cleanup(); };

  // Accessed on any main and on the owning threads.
  RefPtr<dom::ThreadSafeWorkerRef> mWorkerRef;

  // Accessed only on the owning thread.
  nsWeakPtr mGlobal;
  RefPtr<dom::Function> mCallback;

  // Used to make sure we are not going to release the
  // instance on the worker thread, while we are in the
  // process of forwarding a call from the main thread.
  Mutex mMutex;
};

// A WorkerRunnable subclass used to call an ExtensionEventListener
// in the thread that owns the dom::Function wrapped by the
// ExtensionEventListener class.
class ExtensionListenerCallWorkerRunnable : public dom::WorkerRunnable {
 public:
  ExtensionListenerCallWorkerRunnable(
      const RefPtr<ExtensionEventListener>& aExtensionEventListener,
      UniquePtr<dom::StructuredCloneHolder> aArgsHolder)
      : WorkerRunnable(aExtensionEventListener->GetWorkerPrivate(),
                       WorkerThreadUnchangedBusyCount),
        mListener(aExtensionEventListener),
        mArgsHolder(std::move(aArgsHolder)) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aExtensionEventListener);
  }

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  bool WorkerRun(JSContext* aCx, dom::WorkerPrivate* aWorkerPrivate) override;

 private:
  void DeserializeCallArguments(JSContext* aCx, dom::Sequence<JS::Value>& aArg,
                                ErrorResult& aRv);

  RefPtr<ExtensionEventListener> mListener;
  UniquePtr<dom::StructuredCloneHolder> mArgsHolder;
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionEventListener_h
