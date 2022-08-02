/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionAPIBase_h
#define mozilla_extensions_ExtensionAPIBase_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/ErrorResult.h"

class nsIGlobalObject;

namespace mozilla {

namespace dom {
class Function;
}

namespace extensions {

class ExtensionAPIAddRemoveListener;
class ExtensionAPICallFunctionNoReturn;
class ExtensionAPICallSyncFunction;
class ExtensionAPICallAsyncFunction;
class ExtensionAPIGetProperty;
class ExtensionBrowser;
class ExtensionEventManager;
class ExtensionPort;

class ExtensionAPIBase {
 public:
  // WebExtensionStub methods shared between multiple API namespaces.

  virtual void CallWebExtMethodNotImplementedNoReturn(
      JSContext* aCx, const nsAString& aApiMethod,
      const dom::Sequence<JS::Value>& aArgs, ErrorResult& aRv);

  virtual void CallWebExtMethodNotImplementedAsync(
      JSContext* aCx, const nsAString& aApiMethod,
      const dom::Sequence<JS::Value>& aArgs,
      const dom::Optional<OwningNonNull<dom::Function>>& aCallback,
      JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv);

  virtual void CallWebExtMethodNotImplemented(
      JSContext* aCx, const nsAString& aApiMethod,
      const dom::Sequence<JS::Value>& aArgs,
      JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv);

  virtual void CallWebExtMethodNoReturn(JSContext* aCx,
                                        const nsAString& aApiMethod,
                                        const dom::Sequence<JS::Value>& aArgs,
                                        ErrorResult& aRv);
  virtual void CallWebExtMethod(JSContext* aCx, const nsAString& aApiMethod,
                                const dom::Sequence<JS::Value>& aArgs,
                                JS::MutableHandle<JS::Value> aRetVal,
                                ErrorResult& aRv);

  virtual void CallWebExtMethodReturnsString(
      JSContext* aCx, const nsAString& aApiMethod,
      const dom::Sequence<JS::Value>& aArgs, nsAString& aRetVal,
      ErrorResult& aRv);

  virtual already_AddRefed<ExtensionPort> CallWebExtMethodReturnsPort(
      JSContext* aCx, const nsAString& aApiMethod,
      const dom::Sequence<JS::Value>& aArgs, ErrorResult& aRv);

  virtual void CallWebExtMethodAsync(
      JSContext* aCx, const nsAString& aApiMethod,
      const dom::Sequence<JS::Value>& aArgs,
      const dom::Optional<OwningNonNull<dom::Function>>& aCallback,
      JS::MutableHandle<JS::Value> aRetVal, ErrorResult& aRv);

  virtual void CallWebExtMethodAsyncAmbiguous(
      JSContext* aCx, const nsAString& aApiMethod,
      const dom::Sequence<JS::Value>& aArgs,
      JS::MutableHandle<JS::Value> aRetVal, ErrorResult& aRv);

  virtual void GetWebExtPropertyAsString(const nsString& aPropertyName,
                                         dom::DOMString& aRetval);

  virtual void GetWebExtPropertyAsJSValue(JSContext* aCx,
                                          const nsAString& aPropertyName,
                                          JS::MutableHandle<JS::Value> aRetval);

  // API Requests helpers.
  already_AddRefed<ExtensionEventManager> CreateEventManager(
      const nsAString& aEventName);

  RefPtr<ExtensionAPICallFunctionNoReturn> CallFunctionNoReturn(
      const nsAString& aApiMethod);

  RefPtr<ExtensionAPICallSyncFunction> CallSyncFunction(
      const nsAString& aApiMethod);

  RefPtr<ExtensionAPICallAsyncFunction> CallAsyncFunction(
      const nsAString& aApiMethod);

  RefPtr<ExtensionAPIGetProperty> GetProperty(const nsAString& aApiProperty);

  RefPtr<ExtensionAPIAddRemoveListener> SendAddListener(
      const nsAString& aEventName);

  RefPtr<ExtensionAPIAddRemoveListener> SendRemoveListener(
      const nsAString& aEventName);

  static void ThrowUnexpectedError(JSContext* aCx, ErrorResult& aRv);

 protected:
  virtual nsIGlobalObject* GetGlobalObject() const = 0;
  virtual ExtensionBrowser* GetExtensionBrowser() const = 0;
  virtual nsString GetAPINamespace() const = 0;
  virtual nsString GetAPIObjectType() const = 0;
  virtual nsString GetAPIObjectId() const = 0;

 private:
  void CallWebExtMethodAsyncInternal(JSContext* aCx,
                                     const nsAString& aApiMethod,
                                     const dom::Sequence<JS::Value>& aArgs,
                                     const RefPtr<dom::Function>& aCallback,
                                     JS::MutableHandle<JS::Value> aRetval,
                                     ErrorResult& aRv);
};

class ExtensionAPINamespace : public ExtensionAPIBase {
 protected:
  nsString GetAPIObjectType() const override { return VoidString(); }

  nsString GetAPIObjectId() const override { return VoidString(); };
};

class ChromeCompatCallbackHandler final : public dom::PromiseNativeHandler {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  static void Create(ExtensionBrowser* aExtensionBrowser,
                     dom::Promise* aPromise,
                     const RefPtr<dom::Function>& aCallback);

  MOZ_CAN_RUN_SCRIPT void ResolvedCallback(JSContext* aCx,
                                           JS::Handle<JS::Value> aValue,
                                           ErrorResult& aRv) override;
  MOZ_CAN_RUN_SCRIPT void RejectedCallback(JSContext* aCx,
                                           JS::Handle<JS::Value> aValue,
                                           ErrorResult& aRv) override;

 private:
  ChromeCompatCallbackHandler(ExtensionBrowser* aExtensionBrowser,
                              const RefPtr<dom::Function>& aCallback)
      : mCallback(aCallback), mExtensionBrowser(aExtensionBrowser) {
    MOZ_ASSERT(aCallback);
    MOZ_ASSERT(aExtensionBrowser);
  }

  ~ChromeCompatCallbackHandler() = default;

  void ReportUncheckedLastError(JSContext* aCx, JS::Handle<JS::Value> aValue);

  RefPtr<dom::Function> mCallback;
  RefPtr<ExtensionBrowser> mExtensionBrowser;
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionAPIBase_h
