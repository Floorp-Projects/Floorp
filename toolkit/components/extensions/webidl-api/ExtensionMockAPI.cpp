/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionMockAPI.h"
#include "ExtensionEventManager.h"

#include "mozilla/dom/ExtensionMockAPIBinding.h"
#include "mozilla/extensions/ExtensionPort.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace extensions {

NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionMockAPI);
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionMockAPI)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ExtensionMockAPI, mGlobal,
                                      mOnTestEventMgr);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionMockAPI)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ExtensionMockAPI::ExtensionMockAPI(nsIGlobalObject* aGlobal,
                                   ExtensionBrowser* aExtensionBrowser)
    : mGlobal(aGlobal) {
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);
}

/* static */
bool ExtensionMockAPI::IsAllowed(JSContext* aCx, JSObject* aGlobal) {
  return true;
}

JSObject* ExtensionMockAPI::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return dom::ExtensionMockAPI_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* ExtensionMockAPI::GetParentObject() const { return mGlobal; }

void ExtensionMockAPI::GetPropertyAsErrorObject(
    JSContext* aCx, JS::MutableHandle<JS::Value> aRetval) {
  IgnoredErrorResult rv;
  RefPtr<ExtensionAPIGetProperty> request =
      GetProperty(u"propertyAsErrorObject"_ns);
  request->Run(mGlobal, aCx, aRetval, rv);
  if (rv.Failed()) {
    NS_WARNING("ExtensionMockAPI::GetPropertyAsErrorObject failure");
    return;
  }
}

void ExtensionMockAPI::GetPropertyAsString(DOMString& aRetval) {
  IgnoredErrorResult rv;

  dom::AutoJSAPI jsapi;
  if (!jsapi.Init(GetParentObject())) {
    NS_WARNING("ExtensionMockAPI::GetPropertyAsId fail to init jsapi");
  }

  JSContext* cx = jsapi.cx();
  JS::RootedValue retval(cx);

  RefPtr<ExtensionAPIGetProperty> request =
      GetProperty(u"getPropertyAsString"_ns);
  request->Run(mGlobal, cx, &retval, rv);
  if (rv.Failed()) {
    NS_WARNING("ExtensionMockAPI::GetPropertyAsString failure");
    return;
  }
  nsAutoJSString strRetval;
  if (!retval.isString() || !strRetval.init(cx, retval)) {
    NS_WARNING("ExtensionMockAPI::GetPropertyAsString got a non string result");
    return;
  }
  aRetval.SetKnownLiveString(strRetval);
}

ExtensionEventManager* ExtensionMockAPI::OnTestEvent() {
  if (!mOnTestEventMgr) {
    mOnTestEventMgr = CreateEventManager(u"onTestEvent"_ns);
  }

  return mOnTestEventMgr;
}

already_AddRefed<ExtensionPort> ExtensionMockAPI::CallWebExtMethodReturnsPort(
    JSContext* aCx, const nsAString& aApiMethod,
    const dom::Sequence<JS::Value>& aArgs, ErrorResult& aRv) {
  JS::Rooted<JS::Value> apiResult(aCx);
  auto request = CallSyncFunction(u"methodReturnsPort"_ns);
  request->Run(mGlobal, aCx, aArgs, &apiResult, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  IgnoredErrorResult rv;
  RefPtr<ExtensionPort> port = ExtensionPort::Create(mGlobal, apiResult, rv);
  if (NS_WARN_IF(rv.Failed())) {
    // ExtensionPort::Create doesn't throw the js exception with the generic
    // error message as the "api request forwarding" helper classes.
    ThrowUnexpectedError(aCx, aRv);
    return nullptr;
  }

  return port.forget();
}

}  // namespace extensions
}  // namespace mozilla
