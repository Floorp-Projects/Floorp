/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionBrowserSettings_h
#define mozilla_extensions_ExtensionBrowserSettings_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

#include "ExtensionAPIBase.h"
#include "ExtensionBrowser.h"

class nsIGlobalObject;

namespace mozilla::extensions {

class ExtensionBrowserSettingsColorManagement;
class ExtensionEventManager;
class ExtensionSetting;

class ExtensionBrowserSettings final : public nsISupports,
                                       public nsWrapperCache,
                                       public ExtensionAPINamespace {
 public:
  ExtensionBrowserSettings(nsIGlobalObject* aGlobal,
                           ExtensionBrowser* aExtensionBrowser);

  // ExtensionAPIBase methods
  nsIGlobalObject* GetGlobalObject() const override { return mGlobal; }

  ExtensionBrowser* GetExtensionBrowser() const override {
    return mExtensionBrowser;
  }

  nsString GetAPINamespace() const override { return u"browserSettings"_ns; }

  // nsWrapperCache interface methods
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // DOM bindings methods
  static bool IsAllowed(JSContext* aCx, JSObject* aGlobal);

  nsIGlobalObject* GetParentObject() const;

  ExtensionSetting* AllowPopupsForUserEvents();
  ExtensionSetting* CacheEnabled();
  ExtensionSetting* CloseTabsByDoubleClick();
  ExtensionSetting* ContextMenuShowEvent();
  ExtensionSetting* FtpProtocolEnabled();
  ExtensionSetting* HomepageOverride();
  ExtensionSetting* ImageAnimationBehavior();
  ExtensionSetting* NewTabPageOverride();
  ExtensionSetting* NewTabPosition();
  ExtensionSetting* OpenBookmarksInNewTabs();
  ExtensionSetting* OpenSearchResultsInNewTabs();
  ExtensionSetting* OpenUrlbarResultsInNewTabs();
  ExtensionSetting* WebNotificationsDisabled();
  ExtensionSetting* OverrideDocumentColors();
  ExtensionSetting* OverrideContentColorScheme();
  ExtensionSetting* UseDocumentFonts();
  ExtensionSetting* ZoomFullPage();
  ExtensionSetting* ZoomSiteSpecific();

  ExtensionBrowserSettingsColorManagement*
  GetExtensionBrowserSettingsColorManagement();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(ExtensionBrowserSettings)

 private:
  ~ExtensionBrowserSettings() = default;

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<ExtensionBrowser> mExtensionBrowser;
  RefPtr<ExtensionSetting> mAllowPopupsForUserEventsSetting;
  RefPtr<ExtensionSetting> mCacheEnabledSetting;
  RefPtr<ExtensionSetting> mCloseTabsByDoubleClickSetting;
  RefPtr<ExtensionSetting> mContextMenuShowEventSetting;
  RefPtr<ExtensionSetting> mFtpProtocolEnabledSetting;
  RefPtr<ExtensionSetting> mHomepageOverrideSetting;
  RefPtr<ExtensionSetting> mImageAnimationBehaviorSetting;
  RefPtr<ExtensionSetting> mNewTabPageOverrideSetting;
  RefPtr<ExtensionSetting> mNewTabPositionSetting;
  RefPtr<ExtensionSetting> mOpenBookmarksInNewTabsSetting;
  RefPtr<ExtensionSetting> mOpenSearchResultsInNewTabsSetting;
  RefPtr<ExtensionSetting> mOpenUrlbarResultsInNewTabsSetting;
  RefPtr<ExtensionSetting> mWebNotificationsDisabledSetting;
  RefPtr<ExtensionSetting> mOverrideDocumentColorsSetting;
  RefPtr<ExtensionSetting> mOverrideContentColorSchemeSetting;
  RefPtr<ExtensionSetting> mUseDocumentFontsSetting;
  RefPtr<ExtensionSetting> mZoomFullPageSetting;
  RefPtr<ExtensionSetting> mZoomSiteSpecificSetting;
  RefPtr<ExtensionBrowserSettingsColorManagement> mColorManagementNamespace;
};

}  // namespace mozilla::extensions

#endif  // mozilla_extensions_ExtensionBrowserSettings_h
