/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionTest_h
#define mozilla_extensions_ExtensionTest_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/SerializedStackHolder.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

#include "ExtensionAPIBase.h"
#include "ExtensionBrowser.h"

class nsIGlobalObject;

namespace mozilla {

namespace extensions {

class ExtensionEventManager;

bool IsInAutomation(JSContext* aCx, JSObject* aGlobal);

class ExtensionTest final : public nsISupports,
                            public nsWrapperCache,
                            public ExtensionAPINamespace {
 public:
  ExtensionTest(nsIGlobalObject* aGlobal, ExtensionBrowser* aExtensionBrowser);

  // ExtensionAPIBase methods
  nsIGlobalObject* GetGlobalObject() const override { return mGlobal; }

  ExtensionBrowser* GetExtensionBrowser() const override {
    return mExtensionBrowser;
  }

  nsString GetAPINamespace() const override { return u"test"_ns; }

  // nsWrapperCache interface methods
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // DOM bindings methods
  static bool IsAllowed(JSContext* aCx, JSObject* aGlobal);

  nsIGlobalObject* GetParentObject() const;

  void CallWebExtMethodAssertEq(JSContext* aCx, const nsAString& aApiMethod,
                                const dom::Sequence<JS::Value>& aArgs,
                                ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT bool AssertMatchInternal(
      JSContext* aCx, const JS::HandleValue aActualValue,
      const JS::HandleValue aExpectedMatchValue, const nsAString& aMessagePre,
      const nsAString& aMessage,
      UniquePtr<dom::SerializedStackHolder> aSerializedCallerStack,
      ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT void AssertThrows(JSContext* aCx, dom::Function& aFunction,
                                       const JS::HandleValue aExpectedError,
                                       const nsAString& aMessage,
                                       ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT void AssertThrows(JSContext* aCx, dom::Function& aFunction,
                                       const JS::HandleValue aExpectedError,
                                       ErrorResult& aRv);

  void AssertRejects(
      JSContext* aCx, dom::Promise& aPromise,
      const JS::HandleValue aExpectedError, const nsAString& aMessage,
      const dom::Optional<OwningNonNull<dom::Function>>& aCallback,
      JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv);
  void AssertRejects(
      JSContext* aCx, dom::Promise& aPromise,
      const JS::HandleValue aExpectedError,
      const dom::Optional<OwningNonNull<dom::Function>>& aCallback,
      JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv);

  ExtensionEventManager* OnMessage();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(ExtensionTest)

 private:
  ~ExtensionTest() = default;

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<ExtensionBrowser> mExtensionBrowser;
  RefPtr<ExtensionEventManager> mOnMessageEventMgr;
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionTest_h
