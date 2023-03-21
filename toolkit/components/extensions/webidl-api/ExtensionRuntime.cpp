/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionRuntime.h"
#include "ExtensionEventManager.h"

#include "mozilla/dom/ExtensionRuntimeBinding.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace extensions {

NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionRuntime);
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionRuntime)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(
    ExtensionRuntime, mGlobal, mExtensionBrowser, mOnStartupEventMgr,
    mOnInstalledEventMgr, mOnUpdateAvailableEventMgr, mOnConnectEventMgr,
    mOnConnectExternalEventMgr, mOnMessageEventMgr, mOnMessageExternalEventMgr);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionRuntime)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_WEBEXT_EVENTMGR(ExtensionRuntime, u"onStartup"_ns, OnStartup)
NS_IMPL_WEBEXT_EVENTMGR(ExtensionRuntime, u"onInstalled"_ns, OnInstalled)
NS_IMPL_WEBEXT_EVENTMGR(ExtensionRuntime, u"onUpdateAvailable"_ns,
                        OnUpdateAvailable)
NS_IMPL_WEBEXT_EVENTMGR(ExtensionRuntime, u"onConnect"_ns, OnConnect)
NS_IMPL_WEBEXT_EVENTMGR(ExtensionRuntime, u"onConnectExternal"_ns,
                        OnConnectExternal)
NS_IMPL_WEBEXT_EVENTMGR(ExtensionRuntime, u"onMessage"_ns, OnMessage)
NS_IMPL_WEBEXT_EVENTMGR(ExtensionRuntime, u"onMessageExternal"_ns,
                        OnMessageExternal)

ExtensionRuntime::ExtensionRuntime(nsIGlobalObject* aGlobal,
                                   ExtensionBrowser* aExtensionBrowser)
    : mGlobal(aGlobal), mExtensionBrowser(aExtensionBrowser) {
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);
  MOZ_DIAGNOSTIC_ASSERT(mExtensionBrowser);
}

/* static */
bool ExtensionRuntime::IsAllowed(JSContext* aCx, JSObject* aGlobal) {
  return true;
}

JSObject* ExtensionRuntime::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return dom::ExtensionRuntime_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* ExtensionRuntime::GetParentObject() const { return mGlobal; }

void ExtensionRuntime::GetLastError(JSContext* aCx,
                                    JS::MutableHandle<JS::Value> aRetval) {
  mExtensionBrowser->GetLastError(aRetval);
}

void ExtensionRuntime::GetId(dom::DOMString& aRetval) {
  GetWebExtPropertyAsString(u"id"_ns, aRetval);
}

}  // namespace extensions
}  // namespace mozilla
