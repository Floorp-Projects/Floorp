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
                                      mExtensionBrowser, mOnTestEventMgr);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionMockAPI)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_WEBEXT_EVENTMGR_WITH_DATAMEMBER(ExtensionMockAPI, u"onTestEvent"_ns,
                                        OnTestEvent, mOnTestEventMgr)

ExtensionMockAPI::ExtensionMockAPI(nsIGlobalObject* aGlobal,
                                   ExtensionBrowser* aExtensionBrowser)
    : mGlobal(aGlobal), mExtensionBrowser(aExtensionBrowser) {
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);
  MOZ_DIAGNOSTIC_ASSERT(mExtensionBrowser);
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
  ExtensionAPIBase::GetWebExtPropertyAsJSValue(aCx, u"propertyAsErrorObject"_ns,
                                               aRetval);
}

void ExtensionMockAPI::GetPropertyAsString(DOMString& aRetval) {
  GetWebExtPropertyAsString(u"getPropertyAsString"_ns, aRetval);
}

}  // namespace extensions
}  // namespace mozilla
