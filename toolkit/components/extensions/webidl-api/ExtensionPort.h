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
                            public ExtensionAPIBase {
  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<ExtensionEventManager> mOnDisconnectEventMgr;
  RefPtr<ExtensionEventManager> mOnMessageEventMgr;
  UniquePtr<dom::ExtensionPortDescriptor> mPortDescriptor;

  ~ExtensionPort() = default;
  ExtensionPort(nsIGlobalObject* aGlobal,
                UniquePtr<dom::ExtensionPortDescriptor> aPortDescriptor);

 protected:
  // ExtensionAPIBase methods
  nsIGlobalObject* GetGlobalObject() const override { return mGlobal; }

  nsString GetAPINamespace() const override { return u"runtime"_ns; }

  nsString GetAPIObjectType() const override { return u"Port"_ns; }

  nsString GetAPIObjectId() const override;

 public:
  static already_AddRefed<ExtensionPort> Create(
      nsIGlobalObject* aGlobal, JS::Handle<JS::Value> aDescriptorValue,
      ErrorResult& aRv);

  // nsWrapperCache interface methods
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsIGlobalObject* GetParentObject() const;

  ExtensionEventManager* OnDisconnect();
  ExtensionEventManager* OnMessage();

  void GetName(nsAString& aString);
  void GetError(JSContext* aCx, JS::MutableHandle<JSObject*> aResult) {
    // TODO: this is currently just a placeholder, should be filled in
    // with the actual implementation (which may send to the API request
    // handler an API request to get the property value from the port object
    // representation that lives on the main thread).
  }
  void GetSender(JSContext* aCx, JS::MutableHandle<JSObject*> aResult) {
    // TODO: this is currently just a placeholder, needed to please the
    // webidl binding which excepts this property to always return
    // an object.
    aResult.set(JS_NewPlainObject(aCx));
  };

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ExtensionPort)
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionPort_h
