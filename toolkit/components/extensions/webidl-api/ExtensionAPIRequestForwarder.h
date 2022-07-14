/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionAPIRequestForwarder_h
#define mozilla_extensions_ExtensionAPIRequestForwarder_h

#include "ExtensionAPIRequest.h"

#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/ToJSValue.h"

namespace mozilla {
namespace dom {
class ClientInfoAndState;
class Function;
class SerializedStackHolder;
}  // namespace dom
namespace extensions {

class ExtensionAPIRequestForwarder;

// A class used to forward an API request (a method call, add/remote listener or
// a property getter) originated from a WebExtensions global (a window, a
// content script sandbox or a service worker) to the JS privileged API request
// handler available on the main thread (mozIExtensionAPIRequestHandler).
//
// Instances of this class are meant to be short-living, and destroyed when the
// caller function is exiting.
class ExtensionAPIRequestForwarder {
  friend class ExtensionAPIRequest;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ExtensionAPIRequestForwarder)

 public:
  using APIRequestType = mozIExtensionAPIRequest::RequestType;
  using APIResultType = mozIExtensionAPIRequestResult::ResultType;

  static nsresult JSArrayToSequence(JSContext* aCx,
                                    JS::Handle<JS::Value> aJSValue,
                                    dom::Sequence<JS::Value>& aResult);

  static void ThrowUnexpectedError(JSContext* aCx, ErrorResult& aRv);

  static mozIExtensionAPIRequestHandler& APIRequestHandler();

  ExtensionAPIRequestForwarder(const APIRequestType aRequestType,
                               const nsAString& aApiNamespace,
                               const nsAString& aApiMethod,
                               const nsAString& aApiObjectType = u""_ns,
                               const nsAString& aApiObjectId = u""_ns);

  mozIExtensionAPIRequest::RequestType GetRequestType() const {
    return mRequestType;
  }

  const ExtensionAPIRequestTarget* GetRequestTarget() {
    return &mRequestTarget;
  }

  void Run(nsIGlobalObject* aGlobal, JSContext* aCx,
           const dom::Sequence<JS::Value>& aArgs, ErrorResult& aRv);

  void Run(nsIGlobalObject* aGlobal, JSContext* aCx,
           const dom::Sequence<JS::Value>& aArgs,
           ExtensionEventListener* aListener, ErrorResult& aRv);

  void Run(nsIGlobalObject* aGlobal, JSContext* aCx,
           const dom::Sequence<JS::Value>& aArgs,
           JS::MutableHandle<JS::Value> aRetVal, ErrorResult& aRv);

  void Run(nsIGlobalObject* aGlobal, JSContext* aCx,
           const dom::Sequence<JS::Value>& aArgs,
           ExtensionEventListener* aListener,
           JS::MutableHandle<JS::Value> aRetVal, ErrorResult& aRv);

  void Run(nsIGlobalObject* aGlobal, JSContext* aCx,
           const dom::Sequence<JS::Value>& aArgs,
           const RefPtr<dom::Promise>& aPromiseRetval, ErrorResult& aRv);

  void Run(nsIGlobalObject* aGlobal, JSContext* aCx,
           JS::MutableHandle<JS::Value> aRetVal, ErrorResult& aRv);

  void SetSerializedCallerStack(
      UniquePtr<dom::SerializedStackHolder> aCallerStack);

 protected:
  virtual ~ExtensionAPIRequestForwarder() = default;

 private:
  already_AddRefed<ExtensionAPIRequest> CreateAPIRequest(
      nsIGlobalObject* aGlobal, JSContext* aCx,
      const dom::Sequence<JS::Value>& aArgs, ExtensionEventListener* aListener,
      ErrorResult& aRv);

  APIRequestType mRequestType;
  ExtensionAPIRequestTarget mRequestTarget;
  Maybe<UniquePtr<dom::SerializedStackHolder>> mStackHolder;
};

/*
 * This runnable is used internally by ExtensionAPIRequestForwader class
 * to call the JS privileged code that handle the API requests originated
 * from the WebIDL bindings instantiated in a worker thread.
 *
 * The runnable is meant to block the worker thread until we get a result
 * from the JS privileged code that handles the API request.
 *
 * For async API calls we still need to block the worker thread until
 * we get a promise (which we link to the worker thread promise and
 * at that point we unblock the worker thread), because the JS privileged
 * code handling the API request may need to throw some errors synchonously
 * (e.g. in case of additional validations based on the API schema definition
 * for the parameter, like strings that has to pass additional validation
 * or normalizations).
 */
class RequestWorkerRunnable : public dom::WorkerMainThreadRunnable {
 public:
  using APIRequestType = mozIExtensionAPIRequest::RequestType;
  using APIResultType = mozIExtensionAPIRequestResult::ResultType;

  RequestWorkerRunnable(dom::WorkerPrivate* aWorkerPrivate,
                        ExtensionAPIRequestForwarder* aOuterAPIRequest);

  void SetSerializedCallerStack(
      UniquePtr<dom::SerializedStackHolder> aCallerStack);

  /**
   * Init a request runnable for AddListener and RemoveListener API requests
   * (which do have an event callback callback and do not expect any return
   * value).
   */
  void Init(nsIGlobalObject* aGlobal, JSContext* aCx,
            const dom::Sequence<JS::Value>& aArgs,
            ExtensionEventListener* aListener, ErrorResult& aRv);

  /**
   * Init a request runnable for CallFunctionNoReturn API requests (which do
   * do not expect any return value).
   */
  void Init(nsIGlobalObject* aGlobal, JSContext* aCx,
            const dom::Sequence<JS::Value>& aArgs, ErrorResult& aRv) {
    Init(aGlobal, aCx, aArgs, nullptr, aRv);
  }

  /**
   * Init a request runnable for CallAsyncFunction API requests (which do
   * expect a promise as return value).
   */
  void Init(nsIGlobalObject* aGlobal, JSContext* aCx,
            const dom::Sequence<JS::Value>& aArgs,
            const RefPtr<dom::Promise>& aPromiseRetval, ErrorResult& aRv);

  bool MainThreadRun() override;

  void ReadResult(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                  ErrorResult& aRv);

  Maybe<mozIExtensionAPIRequestResult::ResultType> GetResultType() {
    return mResultType;
  }

 protected:
  virtual bool ProcessHandlerResult(JSContext* aCx,
                                    JS::MutableHandle<JS::Value> aRetval);

  already_AddRefed<WebExtensionPolicy> GetWebExtensionPolicy();
  already_AddRefed<ExtensionAPIRequest> CreateAPIRequest(JSContext* aCx);

  void SerializeCallerStack(JSContext* aCx);
  void DeserializeCallerStack(JSContext* aCx,
                              JS::MutableHandle<JS::Value> aRetval);
  void SerializeArgs(JSContext* aCx, const dom::Sequence<JS::Value>& aArgs,
                     ErrorResult& aRv);
  nsresult DeserializeArgs(JSContext* aCx, JS::MutableHandle<JS::Value> aArgs);

  bool HandleAPIRequest(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval);

  Maybe<mozIExtensionAPIRequestResult::ResultType> mResultType;
  Maybe<UniquePtr<dom::StructuredCloneHolder>> mResultHolder;
  RefPtr<dom::PromiseWorkerProxy> mPromiseProxy;
  Maybe<UniquePtr<dom::StructuredCloneHolder>> mArgsHolder;
  Maybe<UniquePtr<dom::SerializedStackHolder>> mStackHolder;
  Maybe<dom::ClientInfo> mClientInfo;
  uint64_t mSWDescriptorId;

  // Only set for addListener/removeListener API requests.
  RefPtr<ExtensionEventListener> mEventListener;

  // The outer request object is kept alive by the caller for the
  // entire life of the inner worker runnable.
  ExtensionAPIRequestForwarder* mOuterRequest;
};

class RequestInitWorkerRunnable : public dom::WorkerMainThreadRunnable {
  Maybe<dom::ClientInfo> mClientInfo;

 public:
  RequestInitWorkerRunnable(dom::WorkerPrivate* aWorkerPrivate,
                            Maybe<dom::ClientInfo>& aSWClientInfo);
  bool MainThreadRun() override;
};

class NotifyWorkerLoadedRunnable : public Runnable {
  uint64_t mSWDescriptorId;
  nsCOMPtr<nsIURI> mSWBaseURI;

 public:
  explicit NotifyWorkerLoadedRunnable(const uint64_t aServiceWorkerDescriptorId,
                                      const nsCOMPtr<nsIURI>& aWorkerBaseURI)
      : Runnable("extensions::NotifyWorkerLoadedRunnable"),
        mSWDescriptorId(aServiceWorkerDescriptorId),
        mSWBaseURI(aWorkerBaseURI) {
    MOZ_ASSERT(mSWDescriptorId > 0);
    MOZ_ASSERT(mSWBaseURI);
  }

  NS_IMETHOD Run() override;

  NS_INLINE_DECL_REFCOUNTING_INHERITED(NotifyWorkerLoadedRunnable, Runnable)

 private:
  ~NotifyWorkerLoadedRunnable() = default;
};

class NotifyWorkerDestroyedRunnable : public Runnable {
  uint64_t mSWDescriptorId;
  nsCOMPtr<nsIURI> mSWBaseURI;

 public:
  explicit NotifyWorkerDestroyedRunnable(
      const uint64_t aServiceWorkerDescriptorId,
      const nsCOMPtr<nsIURI>& aWorkerBaseURI)
      : Runnable("extensions::NotifyWorkerDestroyedRunnable"),
        mSWDescriptorId(aServiceWorkerDescriptorId),
        mSWBaseURI(aWorkerBaseURI) {
    MOZ_ASSERT(mSWDescriptorId > 0);
    MOZ_ASSERT(mSWBaseURI);
  }

  NS_IMETHOD Run() override;

  NS_INLINE_DECL_REFCOUNTING_INHERITED(NotifyWorkerDestroyedRunnable, Runnable)

 private:
  ~NotifyWorkerDestroyedRunnable() = default;
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionAPIRequestForwarder_h
