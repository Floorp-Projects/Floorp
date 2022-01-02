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

void ExtensionRuntime::GetId(DOMString& aRetval) {
  GetWebExtPropertyAsString(u"id"_ns, aRetval);
}

ExtensionEventManager* ExtensionRuntime::OnStartup() {
  if (!mOnStartupEventMgr) {
    mOnStartupEventMgr = CreateEventManager(u"onStartup"_ns);
  }

  return mOnStartupEventMgr;
}

ExtensionEventManager* ExtensionRuntime::OnInstalled() {
  if (!mOnInstalledEventMgr) {
    mOnInstalledEventMgr = CreateEventManager(u"onInstalled"_ns);
  }

  return mOnInstalledEventMgr;
}

ExtensionEventManager* ExtensionRuntime::OnUpdateAvailable() {
  if (!mOnUpdateAvailableEventMgr) {
    mOnUpdateAvailableEventMgr = CreateEventManager(u"onUpdateAvailable"_ns);
  }

  return mOnUpdateAvailableEventMgr;
}

ExtensionEventManager* ExtensionRuntime::OnConnect() {
  if (!mOnConnectEventMgr) {
    mOnConnectEventMgr = CreateEventManager(u"onConnect"_ns);
  }

  return mOnConnectEventMgr;
}

ExtensionEventManager* ExtensionRuntime::OnConnectExternal() {
  if (!mOnConnectExternalEventMgr) {
    mOnConnectExternalEventMgr = CreateEventManager(u"onConnectExternal"_ns);
  }

  return mOnConnectExternalEventMgr;
}

ExtensionEventManager* ExtensionRuntime::OnMessage() {
  if (!mOnMessageEventMgr) {
    mOnMessageEventMgr = CreateEventManager(u"onMessage"_ns);
  }

  return mOnMessageEventMgr;
}

ExtensionEventManager* ExtensionRuntime::OnMessageExternal() {
  if (!mOnMessageExternalEventMgr) {
    mOnMessageExternalEventMgr = CreateEventManager(u"onMessageExternal"_ns);
  }

  return mOnMessageExternalEventMgr;
}

}  // namespace extensions
}  // namespace mozilla
