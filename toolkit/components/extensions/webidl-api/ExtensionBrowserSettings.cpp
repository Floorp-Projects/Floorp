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

ExtensionSetting* ExtensionBrowserSettings::AllowPopupsForUserEvents() {
  if (!mAllowPopupsForUserEventsSetting) {
    mAllowPopupsForUserEventsSetting =
        new ExtensionSetting(mGlobal, mExtensionBrowser,
                             u"browserSettings.allowPopupsForUserEvents"_ns);
  }

  return mAllowPopupsForUserEventsSetting;
}

ExtensionSetting* ExtensionBrowserSettings::CacheEnabled() {
  if (!mCacheEnabledSetting) {
    mCacheEnabledSetting = new ExtensionSetting(
        mGlobal, mExtensionBrowser, u"browserSettings.cacheEnabled"_ns);
  }

  return mCacheEnabledSetting;
}

ExtensionSetting* ExtensionBrowserSettings::CloseTabsByDoubleClick() {
  if (!mCloseTabsByDoubleClickSetting) {
    mCloseTabsByDoubleClickSetting =
        new ExtensionSetting(mGlobal, mExtensionBrowser,
                             u"browserSettings.closeTabsByDoubleClick"_ns);
  }

  return mCloseTabsByDoubleClickSetting;
}

ExtensionSetting* ExtensionBrowserSettings::ContextMenuShowEvent() {
  if (!mContextMenuShowEventSetting) {
    mContextMenuShowEventSetting = new ExtensionSetting(
        mGlobal, mExtensionBrowser, u"browserSettings.contextMenuShowEvent"_ns);
  }

  return mContextMenuShowEventSetting;
}

ExtensionSetting* ExtensionBrowserSettings::FtpProtocolEnabled() {
  if (!mFtpProtocolEnabledSetting) {
    mFtpProtocolEnabledSetting = new ExtensionSetting(
        mGlobal, mExtensionBrowser, u"browserSettings.ftpProtocolEnabled"_ns);
  }

  return mFtpProtocolEnabledSetting;
}

ExtensionSetting* ExtensionBrowserSettings::HomepageOverride() {
  if (!mHomepageOverrideSetting) {
    mHomepageOverrideSetting = new ExtensionSetting(
        mGlobal, mExtensionBrowser, u"browserSettings.homepageOverride"_ns);
  }

  return mHomepageOverrideSetting;
}

ExtensionSetting* ExtensionBrowserSettings::ImageAnimationBehavior() {
  if (!mImageAnimationBehaviorSetting) {
    mImageAnimationBehaviorSetting =
        new ExtensionSetting(mGlobal, mExtensionBrowser,
                             u"browserSettings.imageAnimationBehavior"_ns);
  }

  return mImageAnimationBehaviorSetting;
}

ExtensionSetting* ExtensionBrowserSettings::NewTabPageOverride() {
  if (!mNewTabPageOverrideSetting) {
    mNewTabPageOverrideSetting = new ExtensionSetting(
        mGlobal, mExtensionBrowser, u"browserSettings.newTabPageOverride"_ns);
  }

  return mNewTabPageOverrideSetting;
}

ExtensionSetting* ExtensionBrowserSettings::NewTabPosition() {
  if (!mNewTabPositionSetting) {
    mNewTabPositionSetting = new ExtensionSetting(
        mGlobal, mExtensionBrowser, u"browserSettings.newTabPosition"_ns);
  }

  return mNewTabPositionSetting;
}

ExtensionSetting* ExtensionBrowserSettings::OpenBookmarksInNewTabs() {
  if (!mOpenBookmarksInNewTabsSetting) {
    mOpenBookmarksInNewTabsSetting =
        new ExtensionSetting(mGlobal, mExtensionBrowser,
                             u"browserSettings.openBookmarksInNewTabs"_ns);
  }

  return mOpenBookmarksInNewTabsSetting;
}

ExtensionSetting* ExtensionBrowserSettings::OpenSearchResultsInNewTabs() {
  if (!mOpenSearchResultsInNewTabsSetting) {
    mOpenSearchResultsInNewTabsSetting =
        new ExtensionSetting(mGlobal, mExtensionBrowser,
                             u"browserSettings.openSearchResultsInNewTabs"_ns);
  }

  return mOpenSearchResultsInNewTabsSetting;
}

ExtensionSetting* ExtensionBrowserSettings::OpenUrlbarResultsInNewTabs() {
  if (!mOpenUrlbarResultsInNewTabsSetting) {
    mOpenUrlbarResultsInNewTabsSetting =
        new ExtensionSetting(mGlobal, mExtensionBrowser,
                             u"browserSettings.openUrlbarResultsInNewTabs"_ns);
  }

  return mOpenUrlbarResultsInNewTabsSetting;
}

ExtensionSetting* ExtensionBrowserSettings::WebNotificationsDisabled() {
  if (!mWebNotificationsDisabledSetting) {
    mWebNotificationsDisabledSetting =
        new ExtensionSetting(mGlobal, mExtensionBrowser,
                             u"browserSettings.webNotificationsDisabled"_ns);
  }

  return mWebNotificationsDisabledSetting;
}

ExtensionSetting* ExtensionBrowserSettings::OverrideDocumentColors() {
  if (!mOverrideDocumentColorsSetting) {
    mOverrideDocumentColorsSetting =
        new ExtensionSetting(mGlobal, mExtensionBrowser,
                             u"browserSettings.overrideDocumentColors"_ns);
  }

  return mOverrideDocumentColorsSetting;
}

ExtensionSetting* ExtensionBrowserSettings::OverrideContentColorScheme() {
  if (!mOverrideContentColorSchemeSetting) {
    mOverrideContentColorSchemeSetting =
        new ExtensionSetting(mGlobal, mExtensionBrowser,
                             u"browserSettings.overrideContentColorScheme"_ns);
  }

  return mOverrideContentColorSchemeSetting;
}

ExtensionSetting* ExtensionBrowserSettings::UseDocumentFonts() {
  if (!mUseDocumentFontsSetting) {
    mUseDocumentFontsSetting = new ExtensionSetting(
        mGlobal, mExtensionBrowser, u"browserSettings.useDocumentFonts"_ns);
  }

  return mUseDocumentFontsSetting;
}

ExtensionSetting* ExtensionBrowserSettings::ZoomFullPage() {
  if (!mZoomFullPageSetting) {
    mZoomFullPageSetting = new ExtensionSetting(
        mGlobal, mExtensionBrowser, u"browserSettings.zoomFullPage"_ns);
  }

  return mZoomFullPageSetting;
}

ExtensionSetting* ExtensionBrowserSettings::ZoomSiteSpecific() {
  if (!mZoomSiteSpecificSetting) {
    mZoomSiteSpecificSetting = new ExtensionSetting(
        mGlobal, mExtensionBrowser, u"browserSettings.zoomSiteSpecific"_ns);
  }

  return mZoomSiteSpecificSetting;
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
