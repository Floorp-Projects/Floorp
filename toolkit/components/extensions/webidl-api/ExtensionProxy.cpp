/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionProxy.h"
#include "ExtensionEventManager.h"
#include "ExtensionSetting.h"

#include "mozilla/dom/ExtensionProxyBinding.h"
#include "nsIGlobalObject.h"

namespace mozilla::extensions {

NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionProxy);
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionProxy)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ExtensionProxy, mGlobal,
                                      mExtensionBrowser, mSettingsMgr,
                                      mOnRequestEventMgr, mOnErrorEventMgr);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionProxy)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ExtensionProxy::ExtensionProxy(nsIGlobalObject* aGlobal,
                               ExtensionBrowser* aExtensionBrowser)
    : mGlobal(aGlobal), mExtensionBrowser(aExtensionBrowser) {
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);
  MOZ_DIAGNOSTIC_ASSERT(mExtensionBrowser);
}

/* static */
bool ExtensionProxy::IsAllowed(JSContext* aCx, JSObject* aGlobal) {
  return true;
}

JSObject* ExtensionProxy::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return dom::ExtensionProxy_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* ExtensionProxy::GetParentObject() const { return mGlobal; }

ExtensionSetting* ExtensionProxy::Settings() {
  if (!mSettingsMgr) {
    mSettingsMgr =
        new ExtensionSetting(mGlobal, mExtensionBrowser, u"proxy.settings"_ns);
  }

  return mSettingsMgr;
}

ExtensionEventManager* ExtensionProxy::OnRequest() {
  if (!mOnRequestEventMgr) {
    mOnRequestEventMgr = CreateEventManager(u"onRequest"_ns);
  }

  return mOnRequestEventMgr;
}

ExtensionEventManager* ExtensionProxy::OnError() {
  if (!mOnRequestEventMgr) {
    mOnRequestEventMgr = CreateEventManager(u"onError"_ns);
  }

  return mOnRequestEventMgr;
}

}  // namespace mozilla::extensions
