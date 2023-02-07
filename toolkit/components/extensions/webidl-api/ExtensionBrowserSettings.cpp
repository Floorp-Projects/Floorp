/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionBrowserSettings.h"
#include "ExtensionEventManager.h"
#include "ExtensionSetting.h"
#include "ExtensionBrowserSettingsColorManagement.h"

#include "mozilla/dom/ExtensionBrowserSettingsBinding.h"
#include "nsIGlobalObject.h"

namespace mozilla::extensions {

NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionBrowserSettings);
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionBrowserSettings)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(
    ExtensionBrowserSettings, mGlobal, mExtensionBrowser,
    mAllowPopupsForUserEventsSetting, mCacheEnabledSetting,
    mCloseTabsByDoubleClickSetting, mContextMenuShowEventSetting,
    mFtpProtocolEnabledSetting, mHomepageOverrideSetting,
    mImageAnimationBehaviorSetting, mNewTabPageOverrideSetting,
    mNewTabPositionSetting, mOpenBookmarksInNewTabsSetting,
    mOpenSearchResultsInNewTabsSetting, mOpenUrlbarResultsInNewTabsSetting,
    mWebNotificationsDisabledSetting, mOverrideDocumentColorsSetting,
    mOverrideContentColorSchemeSetting, mUseDocumentFontsSetting,
    mZoomFullPageSetting, mZoomSiteSpecificSetting, mColorManagementNamespace);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionBrowserSettings)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettings, u"allowPopupsForUserEvents"_ns,
                       AllowPopupsForUserEvents)
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettings, u"cacheEnabled"_ns,
                       CacheEnabled)
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettings, u"closeTabsByDoubleClick"_ns,
                       CloseTabsByDoubleClick)
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettings, u"contextMenuShowEvent"_ns,
                       ContextMenuShowEvent)
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettings, u"ftpProtocolEnabled"_ns,
                       FtpProtocolEnabled)
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettings, u"homepageOverride"_ns,
                       HomepageOverride)
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettings, u"imageAnimationBehavior"_ns,
                       ImageAnimationBehavior)
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettings, u"newTabPageOverride"_ns,
                       NewTabPageOverride)
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettings, u"newTabPosition"_ns,
                       NewTabPosition)
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettings, u"openBookmarksInNewTabs"_ns,
                       OpenBookmarksInNewTabs)
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettings,
                       u"openSearchResultsInNewTabs"_ns,
                       OpenSearchResultsInNewTabs)
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettings,
                       u"openUrlbarResultsInNewTabs"_ns,
                       OpenUrlbarResultsInNewTabs)
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettings, u"webNotificationsDisabled"_ns,
                       WebNotificationsDisabled)
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettings, u"overrideDocumentColors"_ns,
                       OverrideDocumentColors)
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettings,
                       u"overrideContentColorScheme"_ns,
                       OverrideContentColorScheme)
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettings, u"useDocumentFonts"_ns,
                       UseDocumentFonts)
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettings, u"zoomFullPage"_ns,
                       ZoomFullPage)
NS_IMPL_WEBEXT_SETTING(ExtensionBrowserSettings, u"zoomSiteSpecific"_ns,
                       ZoomSiteSpecific)

ExtensionBrowserSettings::ExtensionBrowserSettings(
    nsIGlobalObject* aGlobal, ExtensionBrowser* aExtensionBrowser)
    : mGlobal(aGlobal), mExtensionBrowser(aExtensionBrowser) {
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);
  MOZ_DIAGNOSTIC_ASSERT(mExtensionBrowser);
}

/* static */
bool ExtensionBrowserSettings::IsAllowed(JSContext* aCx, JSObject* aGlobal) {
  return true;
}

JSObject* ExtensionBrowserSettings::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return dom::ExtensionBrowserSettings_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* ExtensionBrowserSettings::GetParentObject() const {
  return mGlobal;
}

ExtensionBrowserSettingsColorManagement*
ExtensionBrowserSettings::GetExtensionBrowserSettingsColorManagement() {
  if (!mColorManagementNamespace) {
    mColorManagementNamespace =
        new ExtensionBrowserSettingsColorManagement(mGlobal, mExtensionBrowser);
  }

  return mColorManagementNamespace;
}

}  // namespace mozilla::extensions
