/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionEventListener_h
#define mozilla_extensions_ExtensionEventListener_h

#include "js/Promise.h"  // JS::IsPromiseObject
#include "mozIExtensionAPIRequestHandling.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerPrivate.h"

#include "ExtensionBrowser.h"

class nsIGlobalObject;

namespace mozilla {

namespace dom {
class Function;
}  // namespace dom

namespace extensions {

#define SLOT_SEND_RESPONSE_CALLBACK_INSTANCE 0

// A class that represents a callback parameter passed to WebExtensions API
// addListener / removeListener methods.
//
// Instances of this class are sent to the mozIExtensionAPIRequestHandler as
// a property of the mozIExtensionAPIRequest.
//
// The mozIExtensionEventListener xpcom interface provides methods that allow
// the mozIExtensionAPIRequestHandler running in the Main Thread to call the
// underlying callback Function on its owning thread.
class ExtensionEventListener final : public mozIExtensionEventListener {
 public:
  NS_DECL_MOZIEXTENSIONEVENTLISTENER
  NS_DECL_THREADSAFE_ISUPPORTS

  using CleanupCallback = std::function<void()>;
  using ListenerCallOptions = mozIExtensionListenerCallOptions;
  using APIObjectType = ListenerCallOptions::APIObjectType;
  using CallbackType = ListenerCallOptions::CallbackType;

  static already_AddRefed<ExtensionEventListener> Create(
      nsIGlobalObject* aGlobal, ExtensionBrowser* aExtensionBrowser,
      dom::Function* aCallback, CleanupCallback&& aCleanupCallback,
      ErrorResult& aRv);

  static bool IsPromise(JSContext* aCx, JS::Handle<JS::Value> aValue) {
    if (!aValue.isObject()) {
      return false;
    }

    JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
    return JS::IsPromiseObject(obj);
  }

  dom::WorkerPrivate* GetWorkerPrivate() const;

  RefPtr<dom::Function> GetCallback() const { return mCallback; }

  nsCOMPtr<nsIGlobalObject> GetGlobalObject() const {
    nsCOMPtr<nsIGlobalObject> global = do_QueryReferent(mGlobal);
    return global;
  }

  ExtensionBrowser* GetExtensionBrowser() const { return mExtensionBrowser; }

  void Cleanup() {
    if (mWorkerRef) {
      MutexAutoLock lock(mMutex);

      mWorkerRef->Private()->AssertIsOnWorkerThread();
      mWorkerRef = nullptr;
    }

    mGlobal = nullptr;
    mCallback = nullptr;
    mExtensionBrowser = nullptr;
  }

 private:
  ExtensionEventListener(nsIGlobalObject* aGlobal,
                         ExtensionBrowser* aExtensionBrowser,
                         dom::Function* aCallback)
      : mGlobal(do_GetWeakReference(aGlobal)),
        mExtensionBrowser(aExtensionBrowser),
        mCallback(aCallback),
        mMutex("ExtensionEventListener::mMutex") {
    MOZ_ASSERT(aGlobal);
    MOZ_ASSERT(aExtensionBrowser);
    MOZ_ASSERT(aCallback);
  };

  static UniquePtr<dom::StructuredCloneHolder> SerializeCallArguments(
      const nsTArray<JS::Value>& aArgs, JSContext* aCx, ErrorResult& aRv);

  ~ExtensionEventListener() { Cleanup(); };

  // Accessed on the main and on the owning threads.
  RefPtr<dom::ThreadSafeWorkerRef> mWorkerRef;

  // Accessed only on the owning thread.
  nsWeakPtr mGlobal;
  RefPtr<ExtensionBrowser> mExtensionBrowser;
  RefPtr<dom::Function> mCallback;

  // Used to make sure we are not going to release the
  // instance on the worker thread, while we are in the
  // process of forwarding a call from the main thread.
  Mutex mMutex MOZ_UNANNOTATED;
};

// A WorkerRunnable subclass used to call an ExtensionEventListener
// in the thread that owns the dom::Function wrapped by the
// ExtensionEventListener class.
class ExtensionListenerCallWorkerRunnable : public dom::WorkerRunnable {
  friend class ExtensionListenerCallPromiseResultHandler;

 public:
  using ListenerCallOptions = mozIExtensionListenerCallOptions;
  using APIObjectType = ListenerCallOptions::APIObjectType;
  using CallbackType = ListenerCallOptions::CallbackType;

  ExtensionListenerCallWorkerRunnable(
      const RefPtr<ExtensionEventListener>& aExtensionEventListener,
      UniquePtr<dom::StructuredCloneHolder> aArgsHolder,
      ListenerCallOptions* aCallOptions,
      RefPtr<dom::Promise> aPromiseRetval = nullptr)
      : WorkerRunnable(aExtensionEventListener->GetWorkerPrivate(),
                       WorkerThread),
        mListener(aExtensionEventListener),
        mArgsHolder(std::move(aArgsHolder)),
        mPromiseResult(std::move(aPromiseRetval)),
        mAPIObjectType(APIObjectType::NONE),
        mCallbackArgType(CallbackType::CALLBACK_NONE) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aExtensionEventListener);

    if (aCallOptions) {
      aCallOptions->GetApiObjectType(&mAPIObjectType);
      aCallOptions->GetApiObjectPrepended(&mAPIObjectPrepended);
      aCallOptions->GetCallbackType(&mCallbackArgType);
    }
  }

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  bool WorkerRun(JSContext* aCx, dom::WorkerPrivate* aWorkerPrivate) override;

  bool IsCallResultCancelled() { return mIsCallResultCancelled; }

 private:
  ~ExtensionListenerCallWorkerRunnable() {
    NS_ReleaseOnMainThread("~ExtensionListenerCallWorkerRunnable",
                           mPromiseResult.forget());
    ReleaseArgsHolder();
    mListener = nullptr;
  }

  void ReleaseArgsHolder() {
    if (NS_IsMainThread()) {
      mArgsHolder = nullptr;
    } else {
      auto releaseArgsHolder = [argsHolder = std::move(mArgsHolder)]() {};
      nsCOMPtr<nsIRunnable> runnable =
          NS_NewRunnableFunction(__func__, std::move(releaseArgsHolder));
      NS_DispatchToMainThread(runnable);
    }
  }

  void DeserializeCallArguments(JSContext* aCx, dom::Sequence<JS::Value>& aArg,
                                ErrorResult& aRv);

  RefPtr<ExtensionEventListener> mListener;
  UniquePtr<dom::StructuredCloneHolder> mArgsHolder;
  RefPtr<dom::Promise> mPromiseResult;
  bool mIsCallResultCancelled = false;
  // Call Options.
  bool mAPIObjectPrepended;
  APIObjectType mAPIObjectType;
  CallbackType mCallbackArgType;
};

// A class attached to the promise that should be resolved once the extension
// event listener call has been handled, responsible for serializing resolved
// values or rejected errors on the listener's owning thread and sending them to
// the extension event listener caller running on the main thread.
class ExtensionListenerCallPromiseResultHandler
    : public dom::PromiseNativeHandler {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  static void Create(
      const RefPtr<dom::Promise>& aPromise,
      const RefPtr<ExtensionListenerCallWorkerRunnable>& aWorkerRunnable,
      dom::ThreadSafeWorkerRef* aWorkerRef);

  // PromiseNativeHandler
  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override;
  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override;

  enum class PromiseCallbackType { Resolve, Reject };

 private:
  ExtensionListenerCallPromiseResultHandler(
      dom::ThreadSafeWorkerRef* aWorkerRef,
      RefPtr<ExtensionListenerCallWorkerRunnable> aWorkerRunnable)
      : mWorkerRef(aWorkerRef), mWorkerRunnable(std::move(aWorkerRunnable)) {}

  ~ExtensionListenerCallPromiseResultHandler() = default;

  void WorkerRunCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                         PromiseCallbackType aCallbackType);

  // Set and accessed only on the owning worker thread.
  RefPtr<dom::ThreadSafeWorkerRef> mWorkerRef;

  // Reference to the runnable created on and owned by the main thread,
  // accessed on the worker thread and released on the owning thread.
  RefPtr<ExtensionListenerCallWorkerRunnable> mWorkerRunnable;
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionEventListener_h
