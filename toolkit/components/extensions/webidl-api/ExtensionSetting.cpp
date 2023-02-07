/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionSetting.h"
#include "ExtensionEventManager.h"

#include "mozilla/dom/ExtensionSettingBinding.h"
#include "nsIGlobalObject.h"

namespace mozilla::extensions {

NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionSetting);
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionSetting)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ExtensionSetting, mGlobal,
                                      mExtensionBrowser, mOnChangeEventMgr);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionSetting)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_WEBEXT_EVENTMGR(ExtensionSetting, u"onChange"_ns, OnChange)

ExtensionSetting::ExtensionSetting(nsIGlobalObject* aGlobal,
                                   ExtensionBrowser* aExtensionBrowser,
                                   const nsAString& aNamespace)
    : mGlobal(aGlobal),
      mExtensionBrowser(aExtensionBrowser),
      mAPINamespace(aNamespace) {
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);
  MOZ_DIAGNOSTIC_ASSERT(mExtensionBrowser);
}

/* static */
bool ExtensionSetting::IsAllowed(JSContext* aCx, JSObject* aGlobal) {
  return true;
}

JSObject* ExtensionSetting::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return dom::ExtensionSetting_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* ExtensionSetting::GetParentObject() const { return mGlobal; }

}  // namespace mozilla::extensions
