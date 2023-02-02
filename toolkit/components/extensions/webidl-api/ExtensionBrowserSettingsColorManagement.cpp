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

ExtensionSetting* ExtensionBrowserSettingsColorManagement::Mode() {
  if (!mModeSetting) {
    mModeSetting = new ExtensionSetting(
        mGlobal, mExtensionBrowser, u"browserSettings.colorManagement.mode"_ns);
  }

  return mModeSetting;
}

ExtensionSetting* ExtensionBrowserSettingsColorManagement::UseNativeSRGB() {
  if (!mUseNativeSRGBSetting) {
    mUseNativeSRGBSetting = new ExtensionSetting(
        mGlobal, mExtensionBrowser,
        u"browserSettings.colorManagement.useNativeSRGB"_ns);
  }

  return mUseNativeSRGBSetting;
}

ExtensionSetting*
ExtensionBrowserSettingsColorManagement::UseWebRenderCompositor() {
  if (!mUseWebRenderCompositorSetting) {
    mUseWebRenderCompositorSetting = new ExtensionSetting(
        mGlobal, mExtensionBrowser,
        u"browserSettings.colorManagement.useWebRenderCompositor"_ns);
  }

  return mUseWebRenderCompositorSetting;
}

}  // namespace mozilla::extensions
