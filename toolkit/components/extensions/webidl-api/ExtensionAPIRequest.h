/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionAPIRequest_h
#define mozilla_extensions_ExtensionAPIRequest_h

#include "ExtensionEventListener.h"

#include "mozIExtensionAPIRequestHandling.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/extensions/WebExtensionPolicy.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace extensions {

class ExtensionAPIRequestForwarder;
class RequestWorkerRunnable;

// Represent the target of the API request forwarded, mObjectType and mObjectId
// are only expected to be polulated when the API request is originated from API
// object (like an ExtensionPort returned by a call to browser.runtime.connect).
struct ExtensionAPIRequestTarget {
  nsString mNamespace;
  nsString mMethod;
  nsString mObjectType;
  nsString mObjectId;
};

// A class that represents the service worker that has originated the API
// request.
class ExtensionServiceWorkerInfo : public mozIExtensionServiceWorkerInfo {
 public:
  NS_DECL_MOZIEXTENSIONSERVICEWORKERINFO
  NS_DECL_ISUPPORTS

  explicit ExtensionServiceWorkerInfo(const dom::ClientInfo& aClientInfo,
                                      const uint64_t aDescriptorId)
      : mClientInfo(aClientInfo), mDescriptorId(aDescriptorId) {}

 private:
  virtual ~ExtensionServiceWorkerInfo() = default;

  dom::ClientInfo mClientInfo;
  uint64_t mDescriptorId;
};

// A class that represents a WebExtensions API request (a method call,
// add/remote listener or accessing a property getter) forwarded by the
// WebIDL bindings to the mozIExtensionAPIRequestHandler.
class ExtensionAPIRequest : public mozIExtensionAPIRequest {
 public:
  using APIRequestType = mozIExtensionAPIRequest::RequestType;

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ExtensionAPIRequest)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_MOZIEXTENSIONAPIREQUEST

  explicit ExtensionAPIRequest(
      const mozIExtensionAPIRequest::RequestType aRequestType,
      const ExtensionAPIRequestTarget& aRequestTarget);

  void Init(Maybe<dom::ClientInfo>& aSWClientInfo,
            const uint64_t aSWDescriptorId, JS::Handle<JS::Value> aRequestArgs,
            JS::Handle<JS::Value> aCallerStack);

  static bool ShouldHaveResult(const APIRequestType& aRequestType) {
    switch (aRequestType) {
      case APIRequestType::GET_PROPERTY:
      case APIRequestType::CALL_FUNCTION:
      case APIRequestType::CALL_FUNCTION_ASYNC:
        return true;
      case APIRequestType::CALL_FUNCTION_NO_RETURN:
      case APIRequestType::ADD_LISTENER:
      case APIRequestType::REMOVE_LISTENER:
        break;
      default:
        MOZ_DIAGNOSTIC_ASSERT(false, "Unexpected APIRequestType");
    }

    return false;
  }

  bool ShouldHaveResult() const { return ShouldHaveResult(mRequestType); }

  void SetEventListener(const RefPtr<ExtensionEventListener>& aListener) {
    MOZ_ASSERT(!mEventListener);
    mEventListener = aListener;
  }

 private:
  virtual ~ExtensionAPIRequest() {
    mSWClientInfo = Nothing();
    mArgs.setUndefined();
    mNormalizedArgs.setUndefined();
    mStack.setUndefined();
    mEventListener = nullptr;
    mozilla::DropJSObjects(this);
  };

  APIRequestType mRequestType;
  ExtensionAPIRequestTarget mRequestTarget;
  JS::Heap<JS::Value> mStack;
  JS::Heap<JS::Value> mArgs;
  JS::Heap<JS::Value> mNormalizedArgs;
  Maybe<dom::ClientInfo> mSWClientInfo;
  uint64_t mSWDescriptorId;
  RefPtr<ExtensionServiceWorkerInfo> mSWInfo;

  // Only set for addListener/removeListener API requests.
  RefPtr<ExtensionEventListener> mEventListener;
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionAPIRequest_h
