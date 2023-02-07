/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionBrowserSettingsColorManagement.h"
#include "ExtensionEventManager.h"

#include "mozilla/dom/ExtensionBrowserSettingsColorManagementBinding.h"
#include "nsIGlobalObject.h"

namespace mozilla::extensions {

NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionBrowserSettingsColorManagement);
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionBrowserSettingsColorManagement)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ExtensionBrowserSettingsColorManagement,
                                      mGlobal, mExtensionBrowser, mModeSetting,
                                      mUseNativeSRGBSetting,
                                      mUseWebRenderCompositorSetting);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionBrowserSettingsColorManagement)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettingsColorManagement, u"mode"_ns,
                       Mode);
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettingsColorManagement,
                       u"useNativeSRGB"_ns, UseNativeSRGB);
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettingsColorManagement,
                       u"useWebRenderCompositor"_ns, UseWebRenderCompositor);

ExtensionBrowserSettingsColorManagement::
    ExtensionBrowserSettingsColorManagement(nsIGlobalObject* aGlobal,
                                            ExtensionBrowser* aExtensionBrowser)
    : mGlobal(aGlobal), mExtensionBrowser(aExtensionBrowser) {
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);
  MOZ_DIAGNOSTIC_ASSERT(mExtensionBrowser);
}

/* static */
bool ExtensionBrowserSettingsColorManagement::IsAllowed(JSContext* aCx,
                                                        JSObject* aGlobal) {
  return true;
}

JSObject* ExtensionBrowserSettingsColorManagement::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return dom::ExtensionBrowserSettingsColorManagement_Binding::Wrap(
      aCx, this, aGivenProto);
}

nsIGlobalObject* ExtensionBrowserSettingsColorManagement::GetParentObject()
    const {
  return mGlobal;
}

}  // namespace mozilla::extensions
