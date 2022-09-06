/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionPort_h
#define mozilla_extensions_ExtensionPort_h

#include "js/TypeDecls.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsWrapperCache.h"

#include "ExtensionAPIBase.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {
struct ExtensionPortDescriptor;
}

namespace extensions {

class ExtensionEventManager;

class ExtensionPort final : public nsISupports,
                            public nsWrapperCache,
                            public SupportsWeakPtr,
                            public ExtensionAPIBase {
 public:
  static already_AddRefed<ExtensionPort> Create(
      nsIGlobalObject* aGlobal, ExtensionBrowser* aExtensionBrowser,
      UniquePtr<dom::ExtensionPortDescriptor>&& aPortDescriptor);

  static UniquePtr<dom::ExtensionPortDescriptor> ToPortDescriptor(
      JS::Handle<JS::Value> aDescriptorValue, ErrorResult& aRv);

  // nsWrapperCache interface methods
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsIGlobalObject* GetParentObject() const;

  ExtensionEventManager* OnDisconnect();
  ExtensionEventManager* OnMessage();

  void GetName(nsAString& aString);
  void GetError(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval) {
    GetWebExtPropertyAsJSValue(aCx, u"error"_ns, aRetval);
  }
  void GetSender(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval) {
    GetWebExtPropertyAsJSValue(aCx, u"sender"_ns, aRetval);
  };

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(ExtensionPort)

 protected:
  // ExtensionAPIBase methods
  nsIGlobalObject* GetGlobalObject() const override { return mGlobal; }

  ExtensionBrowser* GetExtensionBrowser() const override {
    return mExtensionBrowser;
  }

  nsString GetAPINamespace() const override { return u"runtime"_ns; }

  nsString GetAPIObjectType() const override { return u"Port"_ns; }

  nsString GetAPIObjectId() const override;

 private:
  ExtensionPort(nsIGlobalObject* aGlobal, ExtensionBrowser* aExtensionBrowser,
                UniquePtr<dom::ExtensionPortDescriptor>&& aPortDescriptor);

  ~ExtensionPort() = default;

  void ForgetReleasedPort();

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<ExtensionBrowser> mExtensionBrowser;
  RefPtr<ExtensionEventManager> mOnDisconnectEventMgr;
  RefPtr<ExtensionEventManager> mOnMessageEventMgr;
  UniquePtr<dom::ExtensionPortDescriptor> mPortDescriptor;
  RefPtr<dom::Function> mCallback;
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionPort_h
