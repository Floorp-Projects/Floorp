---
layout: page
title: Changelog
permalink: /changelog/
---

# 74.0.0-SNAPSHOT (In Development)

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v74.0.0...master)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/135?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/master/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/master/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/master/.config.yml)

* **feature-downloads**:
  * 🚒 Bug fixed [issue #9821](https://github.com/mozilla-mobile/android-components/issues/9821) - Crash for downloads inferred empty mime types.

* **intent-processing**
  * 🌟️ Added support for opening links from ACTION_MAIN Intents. This Intents could the result of `Intent.makeMainSelectorActivity(Intent.ACTION_MAIN, Intent.CATEGORY_APP_BROWSER)` calls.

* **browser-toolbar**
  * 🌟️ Added `ToolbarBehaviorController` to automatically block the `BrowserToolbar` being animated by the `BrowserToolbarBehavior` while the tab is loading. This new class just has to be initialized by AC clients, similar to `ToolbarPresenter`.

* **feature-downloads**:
  * 🚒 Bug fixed [issue #9757](https://github.com/mozilla-mobile/android-components/issues/9757) - Remove downloads notification when private tabs are closed.
  * 🚒 Bug fixed [issue #9789](https://github.com/mozilla-mobile/android-components/issues/9789) - Canceled first PDF download prevents following attempts from downloading.
  * 🚒 Bug fixed [issue #9823](https://github.com/mozilla-mobile/android-components/issues/9823) - Downloads prompts do not show again when a user denies system permission twice.

* **concept-engine**,**browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**, **browser-engine-system**
  * ⚠️ **This is a breaking change**: `EngineSession`.`enableTrackingProtection()` and `EngineSession`.`disableTrackingProtection()` have been removed, please use `EngineSession`.`updateTrackingProtection()` instead , for more details see [issue #9787](https://github.com/mozilla-mobile/android-components/issues/9787).

* **feature-push**
  * ⚠️ **This is a breaking change**: Removed `databasePath` from `RustPushConnection` constructor and added `context`. The file path is now queries lazily.

* **feature-top-sites**
  * ⚠️ **This is a breaking change**: Replace `TopSitesUseCases.renameTopSites` with `TopSitesUseCases.updateTopSites` which allows for updating the title and the url of a top site. [#9599](https://github.com/mozilla-mobile/android-components/issues/9599)

* **service-sync-autofill**
  * Refactors `AutofillCreditCardsAddressesStorage` from **browser-storage-sync** into its own component. [#9801](https://github.com/mozilla-mobile/android-components/issues/9801)

# 73.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v72.0.0...v73.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/134?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v73.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v73.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v73.0.0/.config.yml)

* **All components**
  * ⚠️Increased `targetSdkVersion` to 30 (Android R)

* **browser-toolbar**
  * 🌟 Added `BrowserToolbarBehavior#forceCollapse` to easily collapse the top/bottom toolbar.

* **browser-toolbar**
  * ⚠️ **This is a breaking change**: `BrowserToolbarBottomBehavior` is renamed to `BrowserToolbarBehavior` as it is now a common behavior for toolbars be them placed at the bottom or at the top of the screen.

* **feature-session**
  * ⚠️ **This is a breaking change**: `EngineViewBottomBehavior` is renamed to `EngineViewBrowserToolbarBehavior` as it is now the glue between `EngineView` and `BrowserToolbar` irrespective of if the toolbar is placed at the bottom oir at the top of the `EngineView`.

* **feature-downloads**:
  * 🌟 New `ShareDownloadFeature` will listen for `AddShareAction` and download, cache locally and then share internet resources.
  * ⚠️ **This is a breaking change**: This is a breaking change with clients expected to create and register a new instance of the this new feature otherwise the "Share image" from the browser contextual menu will do nothing.

* **support-ktx**
  * 🌟 Added `Context.shareMedia` that allows to easily share a specific locally stored file through the Android share menu.

* **feature-downloads**:
  * 🚒 Bug fixed [issue #9441](https://github.com/mozilla-mobile/android-components/issues/9441) - Don't ask for redundant system files permission if not required.
  * 🚒 Bug fixed [issue #9526](https://github.com/mozilla-mobile/android-components/issues/9526) - Downloads with generic content types use the correct file extension.
  * 🚒 Bug fixed [issue #9553](https://github.com/mozilla-mobile/android-components/issues/9553) - Multiple files were unable to be opened after being download.

* **feature-webauthn**
  * 🆕 New component to enable support for WebAuthn specification with `WebAuthnFeature`.

* **feature-awesomebar**
  * added `SearchEngineSuggestionProvider` that offers suggestion(s) for search engines based on user search engine list

* **browser-storage-sync**
  * Added `AutofillCreditCardsAddressesStorage` implementation of the `CreditCardsAddressesStorage` interface back by the application-services' `autofill` library.

* **concept-engine**
  * Added `defaultSettings: Settings?` parameter to registerTabHandler to supply a default Tracking Policy when opening a new extension tab.
  * When calling `onNewTab` in `registerTabHandler` from `GeckoWebExtension.kt` a default `TrackingProtectionPolicy.strict()` is supplied to the new `GeckoEngineSession`. This was added in to avoid WebExtension tabs without any ETP settings.

* **concept-storage**
  * Introduced `CreditCardsAddressesStorage` interface for describing credit card and address storage.

* **support-base**
  * Add `NamedThreadFactory`, a `ThreadFactory` that names its threads with the given argument.

* **lib-state**
  * Add `threadNamePrefix` parameter to `Store` to give threads created by the `Store` a specific name.

* **service-glean**
  * 🆙 Updated Glean to version 35.0.0 ([changelog](https://github.com/mozilla/glean/releases/tag/v35.0.0))

# 72.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v71.0.0...v72.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/133?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v72.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v72.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v72.0.0/.config.yml)

* **feature-prompts**:
  * 🚒 Bug fixed [issue #9471](https://github.com/mozilla-mobile/android-components/issues/9471) - Confirm and alert js dialogs don't show "OK" and "Cancel" buttons when the message is too long.

* **support-base**
  * ⚠️ **This is a breaking change**: Update the signature of `ActivityResultHandler.onActivityResult` to avoid conflict with internal Android APIs.

* **feature-addons**
  * 🚒 Bug fixed [issue #9484] https://github.com/mozilla-mobile/android-components/issues/9484) - Handle multiple add-ons update that require new permissions.

* **feature-top-sites**
  * ⚠️ **This is a breaking change**: Replaces `includeFrecent` with an optional `frecencyConfig` in `TopSitesConfig` and `TopSitesStorage.getTopSites` to specify the frecency threshold for the returned list of top frecent sites see [#8690](https://github.com/mozilla-mobile/android-components/issues/8690).

* **service-nimbus**
 * Upgraded nimbus-sdk to enable `getExperimentBranch()` (and friends) to be callable from the main thread.
 * Split up `updateExperiments()` in to two methods: `fetchExperiments()` and `applyPendingExperiments()`.
 * Exposed `setExperimentsLocally()` for testing and startup.

# 71.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v70.0.0...v71.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/132?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v71.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v71.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v71.0.0/.config.yml)

* **feature-prompts**:
  * 🚒 Bug fixed [issue #9351] Camera images are available even with "Don't keep activities" enabled.

* **ui-autocomplete**:
  * Pasting from the clipboard now cleans up any unwanted uri schemes.

* **support-utils**:
  * 🌟 Added SafeUrl#stripUnsafeUrlSchemes that can cleanup unwanted uri schemes. Interested clients can specify what these are by overwriting "mozac_url_schemes_blocklist".

* **concept-fetch**:
  * 🌟 Added `Request#private` to allow requests to be performed in a private context, the feature is not support in all `Client`s check support before using.

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * 🌟 Added support for `Request#private`.

* **feature-prompts**:
  * 🚒 Bug fixed [issue #9229](https://github.com/mozilla-mobile/android-components/issues/9229) - Dismiss SelectLoginPrompt from the current tab when opening a new one ensuring the new one can show it's own. When returning to the previous tab users should focus a login field to see the SelectLoginPrompt again.
  * PromptFeature now implements UserInteractionHandler.onBackPressed to dismiss loginPicker.

* **browser-session**
  * 🚒 Bug fixed [issue #9445](https://github.com/mozilla-mobile/android-components/issues/9445) - LinkEngineSessionAction does not consider restoreState result.

* **feature-contextmenu**
  * 🌟 New functionality [issue #9392](https://github.com/mozilla-mobile/android-components/issues/9392) - Add share and copy link context menu options to images.

* **feature-customtabs**
  * ⚠️ **This is a breaking change**: Multiple breaking changes after migrating `feature-customtabs` to use the browser store, `CustomTabIntentProcessor` requires `AddCustomTabUseCase`,  `CustomTabsToolbarFeature` requires the `BrowserStore` and `CustomTabsUseCases` for more details see [issue #4257](https://github.com/mozilla-mobile/android-components/issues/4257).

* **feature-intent**
  * ⚠️ **This is a breaking change**: Multiple breaking changes after migrating `feature-intent` to use the browser store, `TabIntentProcessor` requires `TabsUseCases` and removes the `openNewTab` parameter, for more details see [issue #4279](https://github.com/mozilla-mobile/android-components/issues/4279).

* **feature-tabs**
  * ⚠️ **This is a breaking change**: `TabsUseCases` now requires `SessionStorage` for more info see [issue #9323](https://github.com/mozilla-mobile/android-components/issues/9323).

* **browser-icons**
  * 🚒 Bug fixed [issue #7888](https://github.com/mozilla-mobile/android-components/issues/7888) - Fixed crash when fetching icon with invalid URI scheme.

* **feature-media**
  * 🚒 Bug fixed [issue #9243](https://github.com/mozilla-mobile/android-components/issues/9243) - Pausing YouTube Video for A While Causes Media Notification to Disappear.
  * 🚒 Bug fixed [issue #9254](https://github.com/mozilla-mobile/android-components/issues/9254) - Headphone control does not pause or play video.

* **support-webextensions**
  * 🚒 Bug fixed [issue #9210](https://github.com/mozilla-mobile/android-components/issues/9210) - White page shown in custom tab when ublock blocks page.

* **feature-downloads**:
  * Allow browsers to change the download notification accent color by providing `Style()` in `AbstractFetchDownloadService`, for more information see [#9299](https://github.com/mozilla-mobile/android-components/issues/9299).

* **feature-accounts-push**
  * Rolling back to previous behaviour of renewing push registration token when the `subscriptionExpired` flag is observed.

* **browser-icons**
  * Catch `IOException` that may be thrown when deleting icon caches.

* **feature-qr**
  * QR Scanner can now scan inverted QR codes, by decoding inverted source when the decoding the original source fails.

* **feature-sitepermissions**
  * ⚠️ **This is a breaking change**: The `SitePermissions` constructor, now parameter types for `autoplayAudible` and `autoplayInaudible` have changed to `AutoplayStatus` as autoplay permissions only support two status `ALLOWED` and `BLOCKED`.

* **feature-app-links**
  * ⚠️ **This is a breaking change**: Migrated this component to use `browser-state` instead of `browser-session`. It is now required to pass a `BrowserStore` instance (instead of `SessionManager`) to `AppLinksFeature`.

* **browser-toolbar**
  * ⚠️ **This is a breaking change**: The API for showing the site permission indicators has been replaced to API to show an dot notification instead, for more information see [#9378](https://github.com/mozilla-mobile/android-components/issues/9378).

* **service-nimbus**
  * Added a `NimbusDisabled` class to provide implementers who are not able to use Nimbus yet.

* **support-base**
  * 🌟 Add an `ActivityResultHandler` for features that want to consume the result.

* **concept-engine**,**browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * 🌟 Added a new `ActivityDelegate` for handling intent requests from the engine.
  * ⚠️ **This is a breaking change**: `EngineSessionState`.`toJSON()` has been removed for more details see [issue #8370](https://github.com/mozilla-mobile/android-components/issues/8370).

* **browser-engine-gecko(-nightly)**
  * Added `GeckoActivityDelegate` for the GeckoView `activityDelegate`.

* **feature-tabs**
  * ⚠️ **This is a breaking change**: Removed the `TabCounterToolbarButton#privateColor` attribute which are replaced by the `tabCounterTintColor` Android styleable attribute.

* **lib-nearby**, **feature-p2p**, **samples-nearby**
  * ⚠️ **This is a breaking change**: Removed Nearby component and related samples.

# 70.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v69.0.0...v70.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/131?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v70.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v70.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v70.0.0/.config.yml)

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * **Merge day!**
    * `browser-engine-gecko-release`: GeckoView 84.0
    * `browser-engine-gecko-beta`: GeckoView 85.0
    * `browser-engine-gecko-nightly`: GeckoView 86.0

* **browser-toolbar**
  * 🌟 Added API to show site permission indicators for more information see [#9131](https://github.com/mozilla-mobile/android-components/issues/9131).

* **browser-awesomebar**:
    * Awesomebar can now be customized for bottom toolbar using the [customizeForBottomToolbar] property

* **service-numbus**
  * Added a `NimbusDisabled` class to provide implementers who are not able to use Nimbus yet.
# 69.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v68.0.0...v69.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/130?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v69.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v69.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v69.0.0/.config.yml)

* **browser-session**
  * ⚠️ **This is a breaking change**: `Session.searchTerms` has been removed. Use `ContentState.searchTerms` (`browser-state`) instead.
  * ⚠️ **This is a breaking change**: `Session.desktopMode` has been removed. Use `ContentState.desktopMode` (`browser-state`) instead.

* **feature-search**
  * ⚠️ **This is a breaking change**: Use cases in `SearchUseCases` now take the ID of s session/tab as parameter instead of a `Session` instance.
  * ⚠️ **This is a breaking change**: `SearchUseCases` no longer depends on `SessionManager` and requires a `TabsUseCases` instance now.

* **feature-tabs**
  * ⚠️ **This is a breaking change**: Use cases in `TabsUseCases` that previously returned a `Session` instance now return the ID of the `Session`.

* **support-test-libstate**
  * Added `CaptureActionsMiddleware`: A `Middleware` implementation for unit tests that want to inspect actions dispatched on a `Store`.

* **feature-sitepermissions**
  * ⚠️ **This is a breaking change**: The `SitePermissionsRules` constructor, now requires a new parameter `mediaKeySystemAccess`.
  * 🌟 Added support for EME permission prompts, see [#7121](https://github.com/mozilla-mobile/android-components/issues/7121).

* **browser-thumbnail**
  * Catch `IOException` that may be thrown when deleting thumbnails.

* **browser-tabstray**
  * ⚠️ **This is a breaking change**: Support temporary hiding if a tab is selected. For this you should use partial bindings with PAYLOAD_(DONT_)HIGHLIGHT_SELECTED_ITEM; override TabsAdapter#isTabSelected for new item bindings; override TabViewHolder#updateSelectedTabIndicator.

* **concept-tabstray**:
  * ⚠️ **This is a breaking change**: A new method - `isTabSelected` was added that exposes to clients an easy way to change what a "selected" tab means.

# 68.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v67.0.0...v68.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/129?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v68.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v68.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v68.0.0/.config.yml)

* ℹ️ Note that various AndroidX dependencies have been updated in this release.

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * 🚒 Bug fixed [issue #8464](https://github.com/mozilla-mobile/android-components/issues/8464) - Crash when confirming a prompt that was already confirmed

* **feature-downloads**
  * 🚒 Bug fixed [issue #9033](https://github.com/mozilla-mobile/android-components/issues/9033) - Fix resuming downloads in slow networks more details see the [Fenix issue](https://github.com/mozilla-mobile/fenix/issues/9354#issuecomment-731267368).
  * 🚒 Bug fixed [issue #9073](https://github.com/mozilla-mobile/android-components/issues/9073) - Fix crash downloading a file with multiple dots on it, for more details see the [Fenix issue](https://github.com/mozilla-mobile/fenix/issues/16443).

* **feature-session**
  * 🚒 Bug fixed [issue #9109](https://github.com/mozilla-mobile/android-components/issues/9109) - Tracking protection shield not getting updated after deleting exception by url [Fenix issue](https://github.com/mozilla-mobile/fenix/issues/16670).

* **feature-app-links**
  * Added handling of PackageItemInfo.packageName NullPointerException on some Xiaomi and TCL devices

* **service-glean**
  * 🆙 Updated Glean to version 33.1.2 ([changelog](https://github.com/mozilla/glean/releases/tag/v33.1.2))

* **feature-tab-collections**:
    * [createCollection] now returns the id of the newly created collection

* **browser-errorpages**:
  * ⚠️ The url encoded error page - `error_page_js` is now clean of inline code. Clients should not rely on inlined code anymore.
  * ⚠️ **This is a breaking change**: Removed the data url encoded error page - `error_pages` which was already deprecated by `error_page_js`.

# 67.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v66.0.0...v67.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/128?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v67.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v67.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v67.0.0/.config.yml)

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * **Merge day!**
    * `browser-engine-gecko-release`: GeckoView 83.0
    * `browser-engine-gecko-beta`: GeckoView 84.0
    * `browser-engine-gecko-nightly`: GeckoView 85.0

* **feature-sitepermissions**
  * 🚒 Bug fixed [issue #8943](https://github.com/mozilla-mobile/android-components/issues/8943) Refactor `SwipeRefreshFeature` to not use `EngineSession.Observer`, this result in multiple site permissions bugs getting fixed see [fenix#8987](https://github.com/mozilla-mobile/fenix/issues/8987) and [fenix#16411](https://github.com/mozilla-mobile/fenix/issues/16411).

* **feature-prompts**
  * 🚒 Bug fixed [issue #8967](https://github.com/mozilla-mobile/android-components/issues/8967) Crash when trying to upload a file see [fenix#16537](https://github.com/mozilla-mobile/fenix/issues/16537), for more information.
  * 🚒 Bug fixed [issue #8953](https://github.com/mozilla-mobile/android-components/issues/8953) - Scroll to selected prompt choice if one exists.

* **feature-accounts-push**
  * ⚠️ `FxaPushSupportFeature` now re-subscribes to push instead of triggering the registration renewal process - this is a temporary workaround and will be removed in the future, see [#7143](https://github.com/mozilla-mobile/android-components/issues/7143).
  * `FxaPushSupportFeature` now takes an optional crash reporter in the constructor.

# 66.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v65.0.0...v66.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/127?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v66.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v66.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v66.0.0/.config.yml)

* **accounts-push**
  * 🚒 Bug fixed [issue #8745](https://github.com/mozilla-mobile/android-components/issues/8745) - Remove OneTimeFxaPushReset from FxaPushSupportFeature
    * ⚠️ **This is a breaking change** because the public API changes with the removal of the class.

* **feature-downloads**
  * 🚒 Bug fixed [issue #8904](https://github.com/mozilla-mobile/android-components/issues/8904) - Fix resuming downloads in nightly/beta more details see the [Fenix issue](https://github.com/mozilla-mobile/fenix/issues/9354).

* **feature-search**
  * ⚠️ **This is a breaking change**: `SearchUseCases` no longer requires a `Context` parameter in the constructor.

* **feature-sitepermissions**
  * ⚠️ **This is a breaking change**: The `SitePermissionsFeature`'s constructor does not requires a `sessionManager` parameter anymore pass a `store` instead.

* **browser-session**
  * `SelectionAwareSessionObserver` is now deprecated. All session state changes can be observed using the browser store (`browser-state` module).

* **feature-addons**
  * `AddonManager.getAddons()` now accepts a new (optional) `allowCache` parameter to configure whether or not a cached response may be returned. This is useful in case a UI flow needs the most up-to-date addons list, or to support "refresh" functionality. By default, cached responses are allowed.

* **service-nimbus**
  * Added new Nimbus rapid experiment library component. This is a Rust based component that is delivered to A-C through the appservices megazord. See the [nimbus-sdk repo](https://github.com/mozilla/nimbus-sdk) for more info.

# 65.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v64.0.0...v65.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/126?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v65.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v65.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v65.0.0/.config.yml)

* **feature-tabs**
  * Added `TabsUseCases.RemoveTabsUseCase` for removing an arbitrary list of tabs.

* **browser-state**
  * Added `TabListAction.RemoveTabsAction` to `BrowserAction`.

* **feature-downloads**
  * 🚒 Bug fixed [issue #8823](https://github.com/mozilla-mobile/android-components/issues/8823) Downloads for data URLs were failing on nightly and beta more details in the [Fenix issue](https://github.com/mozilla-mobile/fenix/issues/16228#issuecomment-717976737).
  * 🚒 Bug fixed [issue #8847](https://github.com/mozilla-mobile/android-components/issues/8847) crash when trying to download a file and switching from a normal to a private mode.
  * 🚒 Bug fixed [issue #8857](https://github.com/mozilla-mobile/android-components/issues/8857)  Sometimes it is not possible to dismiss download notifications see [Fenix#15527](https://github.com/mozilla-mobile/fenix/issues/15527) for more information.

* **feature-sitepermissions**
  * ⚠️ **This is a breaking change**: The `SitePermissionsFeature`'s constructor, now requires a new parameter `BrowserStore` object.
  * 🌟 Moved sitePermissionsFeature from using session to using kotlin flow for observing content and app permission requests[#8554](https://github.com/mozilla-mobile/android-components/issues/8554)

* **feature-addons**
  * 🌟️ Added dividers for sections in add-ons list, see [#8703](https://github.com/mozilla-mobile/android-components/issues/8703).

* **feature-top-sites**
  * Added `RenameTopSiteUseCase` to rename pinned site entries. [#8751](https://github.com/mozilla-mobile/android-components/issues/8751)

* **browser-engine-gecko-nightly**
  * Adds optional `PreferredColorScheme` param to `GeckoEngineView`
  * On `GeckoView` init call `coverUntilFirstPaint()` with `PreferredColorScheme`

* **feature-push**
  * Added `disableRateLimit` to `PushConfig` to allow for easier debugging of push during development.
  * Made `AutoPushFeature` aware of the `PushConfig.disableRateLimit` flag.

* **feature-accounts-push**
  * Made `FxaPushSupportFeature` aware of the `PushConfig.disableRateLimit` flag.

* **support-base**
  * Add `LazyComponent`, a wrapper around `lazy` that avoids initializing recursive dependencies until component use.

# 64.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v63.0.0...v64.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/125?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v64.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v64.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v64.0.0/.config.yml)

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * Exposes GeckoView `CompositorController#ClearColor` as Setting

* **browser-engine-system**
  * ⚠️ **This is a breaking change**: Renames `blackListFile` to `blocklistFile`.
  * ⚠️ **This is a breaking change**: Renames `whiteListFile` to `safelistFile`.

* **feature-addons**
  * 🚒 Bug fixed [issue #7879](https://github.com/mozilla-mobile/android-components/issues/7879) Crash when the default locale is not part of the translations fields of an add-on
  * ⚠️ Removed `Addon.translatedName`, `Addon.translatedSummary` and `Addon.translatedDescription` and added `Addon.translateName(context: Context)`, `Addon.translateSummary(context: Context)` and `Addon.translateDescription(context: Context)`

* **concept-engine**
  * ⚠️ Removed `TrackingCategory`.`SHIMMED`, for user usability reasons, we are going to mark SHIMMED categories as blocked, to follow the same pattern as Firefox desktop for more information see [#8769](https://github.com/mozilla-mobile/android-components/issues/8769)

* **feature-downloads**
  * 🚒 Bug fixed [issue #8585](https://github.com/mozilla-mobile/android-components/issues/8784) create download directory when it doesn't exists for more information see [mozilla-mobile/fenix#15527](https://github.com/mozilla-mobile/fenix/issues/5829).

# 63.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v62.0.0...v63.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/124?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v63.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v63.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v63.0.0/.config.yml)

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * **Merge day!**
    * `browser-engine-gecko-release`: GeckoView 82.0
    * `browser-engine-gecko-beta`: GeckoView 83.0
    * `browser-engine-gecko-nightly`: GeckoView 84.0

* **feature-addons**
  * 🚒 Bug fixed [issue #8681](https://github.com/mozilla-mobile/android-components/issues/8681) Fenix was consuming a lot of extra space on disk, when an add-on update requires a new permission, more info can be found [here](https://github.com/mozilla-mobile/android-components/issues/8681)
  ```

* All components
 * Updated to Kotlin 1.4.10 and Coroutines 1.3.9.
 * Updated to Android Gradle plugin 4.0.1 (downstream projects need to update too).

* **service-glean**
  * Glean was upgraded to v33.0.0
    * ⚠️ **This is a breaking change**: Updated to the Android Gradle Plugin v4.0.1 and Gradle 6.5.1. Projects using older versions of these components will need to update in order to use newer versions of the Glean SDK.

* **browser-toolbar**
  * Clear tint of tracking protection icon when animatable

* **concept-engine**
  * Added `MediaSession` for the media session API.
  * 🌟 Added a new `TrackingCategory`.`SHIMMED` to indicate that content that would have been blocked has instead been replaced with a shimmed file. See more on [Fenix #14071](https://github.com/mozilla-mobile/fenix/issues/14071)

* **browser-engine-gecko(-nightly)**
  * Added `GeckoMediaSessionController` and `GeckoMediaSessionDelegate` for the media session API.

* **browser-state**
  * Added `MediaSessionState` to `SessionState`.
  * Added `MediaSessionAction` to `BrowserAction`.

* **feature-sitepermissions**
  * ⚠️ **This is a breaking change**: The `SitePermissionsRules`'s constructor, now requires a new parameter `persistentStorage`.
  * 🌟 Added support for the local storage site permission see [#3153](https://github.com/mozilla-mobile/android-components/issues/3153).

* **browser-toolbar**
  * 🌟 Added API to add a click listener to the iconView.

* **feature-prompts**
  * The repost prompt now has different text and will also dismiss the pull to refresh throbber.

* **lib-push-firebase**
  * Upgrade Firebase Cloud Messaging to v20.3.0.
    * ⚠️ **This is a breaking change**: `RemoteMessage` is now non-null in `onMessageReceived`.

# 62.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v61.0.0...v62.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/122?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v62.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v62.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v62.0.0/.config.yml)

* **browser-state**
  * ⚠️ **This is a breaking change**: `DownloadState` doesn't support [Parcelable](https://developer.android.com/reference/android/os/Parcelable) anymore.

* **feature-downloads**
  * 🚒 Bug fixed [issue #8585](https://github.com/mozilla-mobile/android-components/issues/8585) fixed regression files not been added to the downloads database system.
  * 🌟 Added new use cases for removing individual downloads (`removeDownload`) and all downloads (`removeAllDownloads`).
  * 🌟 Added support for new download [GeckoView API](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.ContentDelegate.html#onExternalResponse-org.mozilla.geckoview.GeckoSession-org.mozilla.geckoview.GeckoResult-).

* **service-glean**
  * Glean was upgraded to v32.4.1
    * Update `glean_parser` to 1.28.6
      * BUGFIX: Ensure Kotlin arguments are deterministically ordered
    * BUGFIX: Transform ping directory size from bytes to kilobytes before accumulating to `glean.upload.pending_pings_directory_size`

* **feature-customtabs**
  * The drawable for the Action button icon in custom tabs is now scaled to 24dp width an 24dp height.

* **support-images**
  * ⚠️ **This is a breaking change**: `ImageLoader` and `ImageRequest` have moved to the `concept-base` component.

* **browser-session**
  * 🚒 Bug fixed: Having a larger number of tabs slows down cold startup see more on [#8535](https://github.com/mozilla-mobile/android-components/issues/8535) and [#7304](https://github.com/mozilla-mobile/android-components/issues/7304).

* **browser-toolbar**
  * 🚒 Bug fixed: Crash IllegalArgumentException BrowserGestureDetector$CustomScrollDetectorListener see more on [#8356](https://github.com/mozilla-mobile/android-components/issues/8356)

* **feature-qr**
  * 🚒 Bug fixed: Pair message show at every scan after you try to login using scan qr see [#8537](https://github.com/mozilla-mobile/android-components/issues/8537)

* **feature-addons**
  * ⚠️ This is a breaking change for call sites that don't rely on named arguments:
    * `AddonCollectionProvider` now supports configuring a custom collection owner (via AMO user ID or name).
    * `AddonCollectionProvider` now supports configuring the sort order of recommended collections, defaulting to sorting descending by popularity
  ```kotlin
   val addonCollectionProvider by lazy {
        AddonCollectionProvider(
            applicationContext,
            client,
            collectionUser = "16314372"
            collectionName = "myCollection",
            sortOption = SortOption.NAME
            maxCacheAgeInMinutes = DAY_IN_MINUTES
        )
    }
  * Temporary add-ons installed via web-ext are no longer displayed as unsupported.

# 61.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v60.0.0...v61.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/121?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v60.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v60.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v60.0.0/.config.yml)

* **browser-session**
  * Added "undo" functionality via `UndoMiddleware`.
* **feature-tabs**
  * Added `TabsUseCases.UndoTabRemovalUseCase` for undoing the removal of tabs.
* **feature-webcompat-reporter**
  * Added the ability to automatically add a screenshot as well as more technical details when submitting a WebCompat report.
* **feature-addons**
  * Temporary add-ons installed via web-ext are no longer displayed as unsupported.
  * 🚒 Bug fixed [issue #8267](https://github.com/mozilla-mobile/android-components/issues/8267) Devtools permission had wrong translation.
  ```
* **concept-menu**
  * 🌟 Added `AsyncDrawableMenuIcon` class to use icons in a menu that will be loaded later.

# 60.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v59.0.0...v60.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/120?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v60.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v60.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v60.0.0/.config.yml)

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * 🚒 Bug fixed [issue #8431](https://github.com/mozilla-mobile/android-components/issues/8431) update `Session.trackerBlockingEnabled` and `SessionState#trackingProtection#enabled` with the initial tracking protection state.
* **feature-tabs**
  * Added `TabsUseCases.removeNormalTabs()` and `TabsUseCases.removePrivateTabs()`.
  * ⚠️ **This is a breaking change**: Removed `TabsUseCases.removeAllTabsOfType()`.
* **browser-session**
  * Added `SessionManager.removeNormalSessions()` and `SessionManager.removePrivateSessions()`.
* **feature-downloads**
  * 🚒 Bug fixed [issue #8456](https://github.com/mozilla-mobile/android-components/issues/8456) Crash SQLiteConstraintException UNIQUE constraint failed: downloads.id (code 1555).
* **service-glean**
  * Glean was upgraded to v32.4.0
    * Allow using quantity metric type outside of Gecko ([#1198](https://github.com/mozilla/glean/pull/1198))
    * Update `glean_parser` to 1.28.5
      * The `SUPERFLUOUS_NO_LINT` warning has been removed from the glinter. It likely did more harm than good, and makes it hard to make metrics.yaml files that pass across different versions of `glean_parser`.
      * Expired metrics will now produce a linter warning, `EXPIRED_METRIC`.
      * Expiry dates that are more than 730 days (~2 years) in the future will produce a linter warning, `EXPIRATION_DATE_TOO_FAR`.
      * Allow using the Quantity metric type outside of Gecko.
      * New parser configs `custom_is_expired` and `custom_validate_expires` added. These are both functions that take the expires value of the metric and return a bool. (See `Metric.is_expired` and `Metric.validate_expires`). These will allow FOG to provide custom validation for its version-based `expires` values.
    * Add a limit of 250 pending ping files. ([#1217](https://github.com/mozilla/glean/pull/1217)).
    * Don't retry the ping uploader when waiting, sleep instead. This avoids a never-ending increase of the backoff time ([#1217](https://github.com/mozilla/glean/pull/1217)).

# 59.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v58.0.0...v59.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/119?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v59.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v59.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v59.0.0/buildSrc/src/main/java/Config.kt)

* **feature-downloads**
  * 🚒 Bug fixed [issue #8354](https://github.com/mozilla-mobile/android-components/issues/8354) Do not restart FAILED downloads.
* **browser-tabstray**
  * Removed the `BrowserTabsTray` that was deprecated in previous releases.
* **service-telemetry**
  * This library has been removed. Please use `service-glean` instead.
* **service-glean**
  * Glean was upgraded to v32.3.2
    * Track the size of the database file at startup ([#1141](https://github.com/mozilla/glean/pull/1141)).
    * Submitting a ping with upload disabled no longer shows an error message ([#1201](https://github.com/mozilla/glean/pull/1201)).
    * BUGFIX: scan the pending pings directories **after** dealing with upload status on initialization. This is important, because in case upload is disabled we delete any outstanding non-deletion ping file, and if we scan the pending pings folder before doing that we may end up sending pings that should have been discarded. ([#1205](https://github.com/mozilla/glean/pull/1205))
    * Move logic to limit the number of retries on ping uploading "recoverable failures" to glean-core. ([#1120](https://github.com/mozilla/glean/pull/1120))
    * The functionality to limit the number of retries in these cases was introduced to the Glean SDK in `v31.1.0`. The work done now was to move that logic to the glean-core in order to avoid code duplication throughout the language bindings.
    * Update `glean_parser` to `v1.28.3`
      * BUGFIX: Generate valid C# code when using Labeled metric types.
      * BUGFIX: Support `HashSet` and `Dictionary` in the C# generated code.
    * Add a 10MB quota to the pending pings storage. ([#1100](https://github.com/mozilla/glean/pull/1110))
    * Handle ping registration off the main thread. This removes a potential blocking call ([#1132](https://github.com/mozilla/glean/pull/1132)).
* **feature-syncedtabs**
  * Added support for indicators to synced tabs `AwesomeBar` suggestions.
* **feature-addons**
  * ⚠️ **This is a breaking change**: The `Addon.translatePermissions` now requires a `context` object and returns a list of localized strings instead of a list of id string resources.
  * 🚒 Bug fixed [issue #8323](https://github.com/mozilla-mobile/android-components/issues/8323) Add-on permission dialog does not prompt for host permissions.

# 58.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v57.0.0...v58.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/118?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v58.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v58.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v58.0.0/buildSrc/src/main/java/Config.kt)

* **feature-recentlyclosed**
  * Added a new [RecentlyClosedTabsStorage] and a [RecentlyClosedMiddleware] to maintain a list of restorable recently closed tabs.

# 57.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v56.0.0...v57.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/117?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v57.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v57.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v57.0.0/buildSrc/src/main/java/Config.kt)

* **feature-search**
  * ⚠️ **This is a breaking change**: `SearchFeature.performSearch` now takes a second parameter.
  * `BrowserStoreSearchAdapter` and `SearchFeature` can now take a `tabId` parameter.

* **feature-top-sites**
  * ⚠️ **This is a breaking change**: Renames `TopSiteStorage` to `PinnedSitesStorage`.
  * ⚠️ **This is a breaking change**: Renames `TopSiteDao` to `PinnedSiteDao`.
  * ⚠️ **This is a breaking change**: Renames `TopSiteEntity` to `PinnedSiteEntity`.
  * ⚠️ **This is a breaking change**: Replaces `TopSite` interface with a new generic `TopSite` data class.
  * Implements TopSitesFeature based on the RFC [0006-top-sites-feature.md](https://github.com/mozilla-mobile/android-components/blob/master/docs/rfcs/0006-top-sites-feature.md).
  * Downloads, redirect targets, reloads, embedded resources, and frames are no longer considered for inclusion in top sites. Please see [this Application Services PR](https://github.com/mozilla/application-services/pull/3505) for more details.

* **lib-push-firebase**
  * Removed non-essential dependency on `com.google.firebase:firebase-core`.

* **lib-crash**
  * Crash report timestamp is now set to when the crash occurred.
  * When breadcrumbs limit is reached, oldest breadcrumbs are dropped.

* **feature-toolbar**
  * Added `ContainerToolbarFeature` to update the toolbar with the container page action whenever the selected tab changes.

* **browser-state**
  * Added `LastAccessMiddleware` to dispatch `TabSessionAction.UpdateLastAccessAction` when a tab is selected.

* **feature-prompts**
  * Replaced generic icon in `LoginDialogFragment` with site icon (keep the generic one as fallback)

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * 🚒 Bug fixed [issue #8240](https://github.com/mozilla-mobile/android-components/issues/8240) Crash when dismissing Share dialog.

* **feature-downloads**
  * ⚠️ **This is a breaking change**: `AndroidDownloadManager.download` returns a `Strings`, `AndroidDownloadManager.tryAgain` requires a `Strings` `id` parameter.
  * ⚠️ **This is a breaking change**: `ConsumeDownloadAction` requires a `Strings` `id` parameter.
  * ⚠️ **This is a breaking change**: `DownloadManager#onDownloadStopped` requires a `Strings` `(DownloadState, Long, Status) -> Unit`.
  * ⚠️ **This is a breaking change**: `DownloadsUseCases.invoke` requires an `Strings` `downloadId` parameter.
  * ⚠️ **This is a breaking change**: `DownloadState.id` has changed its type from `Long` to `String`.
  * ⚠️ **This is a breaking change**: `BrowserState.downloads` has changed it's type from `Map<Long, DownloadState>` to `Map<String, DownloadState>`.
  * 🌟 Added support for persisting/restoring downloads see issue [#7762](https://github.com/mozilla-mobile/android-components/issues/7762).
  * 🌟 Added `DownloadStorage` for querying stored download metadata.
  * 🚒 Bug [issue #8190](https://github.com/mozilla-mobile/android-components/issues/8190) ArithmeticException: divide by zero in Download notification.
  * 🚒 Bug [issue #8363](https://github.com/mozilla-mobile/android-components/issues/8363) IllegalStateException: Not allowed to start service Intent.

* **ui-widgets**
  * 🆕 New VerticalSwipeRefreshLayout that comes to resolve many of the issues of the platform SwipeRefreshLayout and filters out other gestures than swipe down/up.

# 56.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v55.0.0...v56.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/116?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v55.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v55.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v55.0.0/buildSrc/src/main/java/Config.kt)

* **feature-prompts**
  * 🌟 Added optional `LoginPickerView` and `onManageLogins` params to `PromptFeature` for a new [LoginPicker] to display a view for selecting one of multiple matching saved logins to fill into a site.

* **concept-tabstray**
  * 🌟 Added `onTabsUpdated` to `TabsTray.Observer` for notifying observers when one or more tabs have been added/removed.

* **browser-tabstray**
  * 🌟 Added the convenience function `TabsAdapter.doOnTabsUpdated` for performing actions only once when the tabs are updated.

* **feature-app-links**
  * 🚒 Bug fixed [issue #8169](https://github.com/mozilla-mobile/android-components/issues/8169) App links dialog will now call `showNow` to immediately show the dialog.

* **browser-thumbnails**
  * 🌟 Exposed `BrowserThumbnail.requestThumbnail` API for consumers.

* **browser-engine-gecko-nightly**
  * 🌟 Added `onPaintStatusReset` when a session's paint has been reset.
  * 🚒 Bug fixed [issue #8123](https://github.com/mozilla-mobile/android-components/issues/8123) Fix history title crash.

* **browser-engine-gecko-beta and **browser-engine-gecko **
  * 🚒 Bug fixed [issue #8123](https://github.com/mozilla-mobile/android-components/issues/8123) Fix history title crash.

* **concept-menu**
  * 🌟 Added `MenuStyle` class to set menu background and width.

* **browser-menu**
  * 🌟 Added `style` parameter to `BrowserMenu.show`.
  * 🚒 Bug fixed [issue #8223](https://github.com/mozilla-mobile/android-components/issues/8223): Fix compound drawable position for RTL.

* **browser-menu2**
  * 🌟 Added `style` parameter to `BrowserMenuController`.

* **ui-widgets**
  * 🌟 Added widget for showing a website in a list, such as in bookmarks or history. The `mozac_primary_text_color` and `mozac_caption_text_color` attributes should be set.

* **browser-icons**
  * 🌟 Expose `BrowserIcons.clear()` as a public API to remove all saved data from disk and memory caches.

* **feature-downloads**
  * 🚒 Bug fixed [issue #8202](https://github.com/mozilla-mobile/android-components/issues/8202): Download's ui were always showing failed status.

* **feature-pwa**
  * ⚠️ **This is a breaking change**: The `SiteControlsBuilder` interface has changed. `buildNotification` now takes two parameters: `Context` and `Notification.Builder`.
  * `WebAppSiteControlsFeature` now supports displaying monochrome icons.

* **service-glean**
  * Glean was updated to v32.1.1
      * Support installing glean_parser in offline mode.
      * Fix a startup crash on some Android 8 (SDK=25) devices, due to a [bug in the Java compiler](https://issuetracker.google.com/issues/110848122#comment17).

* **feature-readerview**
  * 🚒 Bug fixed [issue #8208](https://github.com/mozilla-mobile/android-components/issues/8208): Add a link to the original page in Reader Mode.

* **feature-addons**
  * 🌟 Feature [issue #8200](https://github.com/mozilla-mobile/android-components/issues/8200): "users" in add-ons manager should be renamed to "reviews".

# 55.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v54.0.0...v55.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/115?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v55.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v55.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v55.0.0/buildSrc/src/main/java/Config.kt)

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * Fixed issue [#7983](https://github.com/mozilla-mobile/android-components/issues/7983), crash when a file name wasn't provided when uploading a file.

* **service-glean**
  * Glean was updated to v32.1.0
    * The rate limiter now allows 15, rather than 10, pings per minute.

* **storage-sync**
  * Fixed [issue #8011](https://github.com/mozilla-mobile/android-components/issues/8011) PlacesConnectionBusy: Error executing SQL: database is locked.

* **feature-app-links**
  * Fixed [issue #8122](https://github.com/mozilla-mobile/android-components/issues/8122) App link redirecting to Play Store, rather than to installed app.

# 54.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v53.0.0...v54.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/113?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v54.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v54.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v54.0.0/buildSrc/src/main/java/Config.kt)

* **concept-menu**
  * Added `orientation` parameter to `MenuController.show`. Passing null (the default) tells the menu to determine the best orientation itself.

* **browser-menu2**
  * ⚠️ **This is a breaking change**: `BrowserMenuController.show` no longer supports the width parameter.

* **feature-app-links**
  * Added `loadUrlUseCase` as a parameter for `AppLinksFeature`.  This is used to load the URL if the user decides to not launch the link in the external app.

* **concept-awesomebar**
  * Added `AwesomeBar.setOnEditSuggestionListener()` to register a callback when a search term is selected to be edited further.

* **browser-toolbar**
  * `BrowserToolbar.setSearchTerms()` can now be called during `State.EDIT`

* **browser-awesomebar**
  * The view of `DefaultSuggestionViewHolder` now contains a button to select a search term for further editing. Clicking it will invoke the callback registered in `BrowserAwesomeBar.setOnEditSuggestionListener()`

* **browser-menu**
  * ⚠️ **This is a breaking change**: Removed `SimpleBrowserMenuHighlightableItem.itemType`. Use a WeakMap instead if you need to attach private data.

* **browser-menu**
  * For a11y, `BrowserMenuImageSwitch` now highlights the entire row, not just the switch

* **concept-engine**
  * Added the `cookiePurging` property to `TrackingProtectionPolicy` and `TrackingProtectionPolicyForSessionTypes` constructors to enable/disable cookie purging feature read more about it [here](https://blog.mozilla.org/blog/2020/08/04/latest-firefox-rolls-out-enhanced-tracking-protection-2-0-blocking-redirect-trackers-by-default/).

* **browser-engine-nightly**
  * Added `cookiePurging` to `TrackingProtectionPolicy.toContentBlockingSetting`.

* **feature-addons**
  * ⚠️ **This is a breaking change**: Unified addons icons design with the one of favicons used all throughout the app. Specifying a different background is not possible anymore.

# 53.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v52.0.0...v53.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/112?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v53.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v53.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v53.0.0/buildSrc/src/main/java/Config.kt)

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * **Merge day!**
    * `browser-engine-gecko`: GeckoView 79.0
    * `browser-engine-gecko-beta`: GeckoView 80.0
    * `browser-engine-gecko-nightly`: GeckoView 81.0

* **feature-downloads**
  * ⚠️ **This is a breaking change**: Removed the following properties from `DownloadJobState` in `AbstractFetchDownloadService` and added to `DownloadState`: `DownloadJobStatus` (now renamed to `DownloadStatus`) and `currentBytesCopied`. These properties can now be read from `DownloadState`.
  * ⚠️ **This is a breaking change**: Removed the enum class `DownloadJobStatus` from `AbstractFetchDownloadService` and moved into `DownloadState`, and removed the `ACTIVE` state while introducing two new states called `INITIATED` and `DOWNLOADING`.
  * ⚠️ **This is a breaking change**: Renamed the following data classes from `BrowserAction`: `QueuedDownloadAction` to `AddDownloadAction`, `RemoveQueuedDownloadAction` to `RemoveDownloadAction`, `RemoveAllQueuedDownloadsAction` to `RemoveAllDownloadsAction`, and `UpdateQueuedDownloadAction` to `UpdateDownloadAction`.
  * ⚠️ **This is a breaking change**: Renamed `queuedDownloads` from `BrowserState` to `downloads` .
  * Removed automatic deletion of `downloads` upon a completed download.
  * 🆕 Added support for choosing a third party app to perform a download.

* **browser-menu**
  * ⚠️ **This is a breaking change**: `BrowserMenuItemToolbar.Button.longClickListener` is now nullable and defaults to null.

* **concept-menu**
  * Added `SmallMenuCandidate.onLongClick` to handle long click of row menu buttons.

* **service-glean**
  * Glean was updated to v31.6.0
    * Limit ping request body size to 1MB. ([#1098](https://github.com/mozilla/glean/pull/1098))
    * BUGFIX: Require activities executed via `GleanDebugView` to be exported.

* **feature-downloads**
  * ⚠️ **This is a breaking change**: `DownloadsFeature` is no longer accepting a custom download dialog but supporting customizations via the `promptStyling` parameter. The `dialog` parameter was unused so far. If it's required in the future it will need to be replaced with a lambda or factory so that the feature can create instances of the dialog itself, as needed.

* **feature-webcompat-reporter**
  * Added a second parameter to the `install` method: `productName` allows to provide a unique product name per usage for automatic product-labelling on webcompat.com

* **feature-contextmenu**
  * Do not show the "Download link" option for html URLs.
  * Uses a speculative check, may not work in all cases.

* **ui-widgets**
  * Added shared ImageView style for favicons. The `mozac_widget_favicon_background_color` and `mozac_widget_favicon_border_color` attributes should be set, then `style="@style/Mozac.Widgets.Favicon"` can be added to an ImageView.

# 52.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v51.0.0...v52.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/111?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v52.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v52.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v52.0.0/buildSrc/src/main/java/Config.kt)

* **browser-icons**
  * Added `BrowserIcons.clear()` to remove all saved data from disk and memory caches.

* **support-images**
  * ⚠️ **This is a breaking change**: Removed `ImageLoader.loadIntoView(view: ImageView, id: String)` extension function.

* **service-glean**
  * Glean was updated to v31.6.0
    * Implement ping tagging (i.e. the `X-Source-Tags` header) ([#1074](https://github.com/mozilla/glean/pull/1074)). Note that this is not yet implemented for iOS.
    * String values that are too long now record `invalid_overflow` rather than `invalid_value` through the Glean error reporting mechanism. This affects the string, event and string list metrics.
    * `metrics.yaml` files now support a `data_sensitivity` field to all metrics for specifying the type of data collected in the field.
    * Allow defining which `Activity` to run next when using the `GleanDebugActivity`.
    * Implement JWE metric type ([#1073](https://github.com/mozilla/glean/pull/1073), [#1062](https://github.com/mozilla/glean/pull/1062)).
    * DEPRECATION: `getUploadEnabled` is deprecated (respectively `get_upload_enabled` in Python) ([#1046](https://github.com/mozilla/glean/pull/1046))
      * Due to Glean's asynchronous initialization the return value can be incorrect.
        Applications should not rely on Glean's internal state.
        Upload enabled status should be tracked by the application and communicated to Glean if it changes.
        Note: The method was removed from the C# and Python implementation.
    * Update `glean_parser` to `v1.28.1`
      * The `glean_parser` linting was leading consumers astray by incorrectly suggesting that `deletion-request` be instead `deletion_request` when used for `send_in_pings`. This was causing metrics intended for the `deletion-request` ping to not be included when it was collected and submitted. Consumers that are sending metrics in the `deletion-request` ping will need to update the `send_in_pings` value in their metrics.yaml to correct this.
      * Fixes a bug in doc rendering.

* **feature-syncedtabs**
  * ⚠️ **This is a breaking change**: Adds context to the constructor of `SyncedTabsFeature`.

* **browser-tabstray**
  * ⚠️ **This is a breaking change**: The `BrowserTabsTray` is now deprecated. Using a `RecyclerView` directly is now recommended.
  * ⚠️ **This is a breaking change**: `ViewHolderProvider` no longer has a second param.
  * ⚠️ **This is a breaking change**: `TabsAdapter` has a `styling` field `TabsTrayStyling`.

* **browser-menu2**
  * Added new component to replace **browser-menu**. menu2 uses immutable data types and better integrates with `BrowserStore`.

* **concept-menu**
  * Added concept component to contain data classes for menu items and interfaces for menu controllers.

* **browser-toolbar**
  * Added support for the new `MenuController` interface for menu2.
    When a menu controller is added to a toolbar, it will be used in place of the `BrowserMenuBuilder`.
    The builder will supply items to the `MenuController` in `invalidateMenu` if it is kept.

* **feature-containers**
  * Adds a `ContainerMiddleware` that connects container browser actions with the `ContainerStorage`.

* **feature-app-links**
  * ⚠️ **This is a breaking change**: add `lastUri` as a parameter for `AppLinksInterceptor`.

* **concept-engine**, **browser-engine-***
  * ⚠️ **This is a breaking change**: add `lastUri` as a parameter for `RequestInterceptor.onLoadRequest`.

* **support-ktx**
  * Added `Vibrator.vibrateOneShot` compat method.

# 51.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v50.0.0...v51.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/110?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v51.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v51.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v51.0.0/buildSrc/src/main/java/Config.kt)

* **browser-session**
  * ⚠️ **This is a breaking change**: Removed the following properties from `Session`: `findResults`, `hitResult`, `fullScreenMode` and `layoutInDisplayCutoutMode`. The values still exist in `BrowserState` and can be read from there.

* **support-images**
  * ⚠️ **This is a breaking change**: Removed `ImageRequest` in favor of `ImageSaveRequest` and `ImageLoadRequest` to separate the information passed to methods that load and save images.
  * ⚠️ **This is a breaking change**: `ImageLoader.loadIntoView()` now takes an `ImageSaveRequest` instead of an `ImageRequest`.

* **browser-thumbnails**
  * ⚠️ **This is a breaking change**: `ThumbnailStorage.saveThumbnail()` now takes an `ImageSaveRequest` instead of an `ImageRequest`.
  * ⚠️ **This is a breaking change**: `ThumbnailStorage.loadThumbnail()` now takes an `ImageLoadRequest` instead of an `ImageRequest`.

* **feature-addons**
    * AddonManager now receives a `WebExtensionRuntime` instead of an `Engine`. This has no impact to consumers of `feature-addons` as `Engine` already implements `WebExtensionRuntime`.

* **browser-session**
  * ⚠️ **This is a breaking change**: `Session.Source` was moved to browser-state and is now `SessionState.Source`. No change is required other than fixing imports.

* **browser-engine-beta**
  * Added `TrackingProtectionPolicy.toContentBlockingSetting` extension methods to convert a policy to GeckoView's `ContentBlocking.Setting`.

* **browser-engine-nightly**
  * Added `TrackingProtectionPolicy.toContentBlockingSetting` extension methods to convert a policy to GeckoView's `ContentBlocking.Setting`.
  * Added checks to not apply the same policy twice if they are the same as already on the engine.

* **feature-pwa**
  * Added `ManifestStorage.loadShareableManifests` to get manifest with a `WebAppManifest.ShareTarget`.

* **support-ktx**
  * Add `Activity.reportFullyDrawnSafe`, a function to call `Activity.reportFullyDrawn` while catching crashes under some circumstances.

# 50.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v49.0.0...v50.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/109?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v50.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v50.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v50.0.0/buildSrc/src/main/java/Config.kt)

* **feature-session**
  * Added `SessionFeature.release()`: Calling this method stops the feature from rendering sessions on the `EngineView` (until explicitly started again) and releases an already rendering session from the `EngineView`.
  * Added `SessionUseCases.goToHistoryIndex` to allow consumers to jump to a specific index in a session's history.
  * Added `flags` parameter to `ReloadUrlUseCase`.

* **support-ktx**
  * Adds `Bundle.contentEquals` function to check if two bundles are equal.

* **concept-engine**
  * Added `EngineSession.goToHistoryIndex` to jump to a specific index in a session's history.
  * Adds `flags` parameter to `reload`.

* **service-glean**
  * Glean was updated to v31.4.1
    * Remove locale from baseline ping. ([1609968](https://bugzilla.mozilla.org/show_bug.cgi?id=1609968), [#1016](https://github.com/mozilla/glean/pull/1016))
    * Persist X-Debug-ID header on store ping. ([1605097](https://bugzilla.mozilla.org/show_bug.cgi?id=1605097), [#1042](https://github.com/mozilla/glean/pull/1042))
    * BUGFIX: raise an error if Glean is initialized with an empty string as the `application_id` ([#1043](https://github.com/mozilla/glean/pull/1043)).
    * Enable debugging features through environment variables. ([#1058](https://github.com/mozilla/glean/pull/1058))
    * BUGFIX: fix `int32` to `ErrorType` mapping. The `InvalidOverflow` had a value mismatch between glean-core and the bindings. This would only be a problem in unit tests. ([#1063](https://github.com/mozilla/glean/pull/1063))
    * Enable propagating options to the main product Activity when using the `GleanDebugActivity`.
    * BUGFIX: Fix the metrics ping collection for startup pings such as `reason=upgrade` to occur in the same thread/task as Glean initialize. Otherwise, it gets collected after the application lifetime metrics are cleared such as experiments that should be in the ping. ([#1069](https://github.com/mozilla/glean/pull/1069))

* **service-location**
  * `LocationService.hasRegionCached()` is introduced to query if the region is already cached and a long running operation to fetch the region is not needed.

* **browser-state**
  * Added map of `SessionState.contextId` and their respective `ContainerState` in `BrowserState` to track the state of a container.

* **browser-menu**
  * Added an optional `longClickListener` parameter to `BrowserMenuItemToolbar.Button` and `BrowserMenuItemToolbar.TwoStateButton` to handle long click events.

# 49.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v48.0.0...v49.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/108?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v49.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v49.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v49.0.0/buildSrc/src/main/java/Config.kt)

* **feature-session**
  * ⚠️ **This is a breaking change**: `FullScreenFeature`, `SettingsUseCases`, `SwipeRefreshFeature` and `SessionFeature` now require a `BrowserStore` instead of a `SessionManager`.
  * ⚠️ **This is a breaking change**: `SessionFeature` now requires an `EngineSessionUseCases` instance.
  * ⚠️ **This is a breaking change**: `TrackingProtectionUseCases` now requires a `BrowserStore` instead of a `SessionManager`.

* **feature-tabs**
  * ⚠️ **This is a breaking change**: `TabsUseCases.AddNewTabUseCase` and `TabsUseCases.AddNewPrivateTabUseCase` now require a `BrowserStore` instead of a `SessionManager`.

* **browser-session**
  * `SessionManager.getEngineSession()` is now deprecated. From now on please read `EngineSession` instances of tabs from `BrowserState`.

* **service-sync-logins**
  * ⚠️ **This is a breaking change**: removed `isAutofillEnabled` lambda from `GeckoLoginStorageDelegate` because setting has been exposed through GV

* **concept-engine**
  * Adds `profiler` property with `isProfilerActive`, `getProfilerTime` and `addMarker` Firefox Profiler APIs. These will allow to add profiler markers.

* **support-ktx**
  * Adds `Resources.getSpanned` to format strings using style spans.
  * Adds `Resources.locale` to get the corresponding locale on all SDK versions.

* **feature-logins**
  * 🆕 New component for logins related features.
  * Adds `LoginExceptionStorage` for storing and accessing save login prompt exceptions.

* **feature-prompts**
   * Added an optional `LoginExceptions` param that a storage layer can implement to check for exceptions before showing a save login prompt.

# 48.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v47.0.0...v48.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/107?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v48.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v48.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v48.0.0/buildSrc/src/main/java/Config.kt)

* **feature-intent**
   * ⚠️ **This is a breaking change**: `IntentProcessor.process` is not a suspend function anymore.

* **feature-search**
  * Adds optional `parentSession` to attach to a new search session in `SearchUseCases`

* **feature-contextmenu**
  * Add "Share image" to context menu.

* **feature-session**
  * ⚠️ **This is a breaking change**: `AbstractCustomTabsService` now requires `CustomTabsServiceStore`.
  * ⚠️ **This is a breaking change**: `httpClient` and `apiKey` have been removed from `AbstractCustomTabsService`. They should be replaced with building `DigitalAssetLinksApi` and passing it to `relationChecker`.
  * Added `relationChecker` to customize Digital Asset Links handling.

* **feature-pwa**
  * ⚠️ **This is a breaking change**: `TrustedWebActivityIntentProcessor` now requires a `RelationChecker` instead of `httpClient` and `apiKey`.
  * ⚠️ **This is a breaking change**: Removed unused API from `WebAppShortcutManager`: uninstallShortcuts
  * `WebAppShortcutManager` gained a new API: recentlyUsedWebAppsCount. Allows counting recently used web apps.

* **browser-thumbnails**
  * The `ThumbnailMiddleware` deletes the tab's thumbnail from the storage when the sessions are removed.
  * `BrowserThumbnails` waits for the `firstContentfulPaint` before requesting a screenshot.

* **feature-tabs**
  * ⚠️ **This is a breaking change**: Removes unused `ThumbnailsUseCases` since we now load thumbnails via `ThumbnailLoader`. See [#7313](https://github.com/mozilla-mobile/android-components/issues/7313).
  * ⚠️ **This is a breaking change**: Removes `ThumbnailsUseCases` as a parameter in `TabsFeature` and `TabsTrayPresenter`.
  * ⚠️ **This is a breaking change**: Change the id parameter to accept a new `ImageRequest` in `ImageLoader`, which
    allows consumers of `ThumbnailLoader` to specify the preferred image size along with the id when loading an image.

* **feature-privatemode**
  * Add `PrivateNotificationFeature` to display a notification when private sessions are open.

* **service-glean**
  * Glean was updated to v31.2.3
    * BUGFIX: Correctly format the date and time in the Date header
    * Feature: Add rate limiting capabilities to the upload manager
    * BUGFIX: baseline pings with reason "dirty startup" are no longer sent if Glean did not full initialize in the previous run
    * BUGFIX: Compile dependencies with `NDEBUG` to avoid linking unavailable symbols.
      This fixes a crash due to a missing `stderr` symbol on older Android.
    * BUGFIX: Fix mismatch in events keys and values by using glean_parser version 1.23.0.

* **feature-webnotifications**
  * `WebNotificationFeature` checks the site permissions first before showing a notification.
  * Notifications with a long text body are now expandable to show the full text.

* **feature-addons**
  * Add `Addon.createdAtDate` and `Addon.updatedAtDate` extensions to get `Addon.createdAt` and `Addon.updatedAt` as a `Date`.

# 47.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v46.0.0...v47.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/106?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v47.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v47.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v47.0.0/buildSrc/src/main/java/Config.kt)

* **service-digitalassetlinks**
  * Added new component for listing and checking Digital Asset Links. Contains a local checker and a version that uses Google's API.

# 46.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v44.0.0...v46.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/105?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v46.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v46.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v46.0.0/buildSrc/src/main/java/Config.kt)

* **concept-engine**
  * Exposed `GeckoRuntimeSettings#setLoginAutofillEnabled` to control whether login forms should be automatically filled in suitable situations

* **browser-icons**
  * Fixed issue [#7142](https://github.com/mozilla-mobile/android-components/issues/7142)

* **feature-downloads**
  * On devices older than Q we were not adding downloads to the system download database for more information see [#7230](https://github.com/mozilla-mobile/android-components/issues/7230)

* **browser-storage-sync**
  * Added `getTopFrecentSites` to `PlacesHistoryStorage`, which returns a list of the top frecent site infos
    sorted by most to least frecent.

* **local development**
  * Enable local Gradle Build Cache to speed-up local builds. Build cache is located in `.build-cache/`, clear it if you run into strange problems and please file an issue.

* **support-rustlog**
  * `RustLog.enable` now takes an optional [CrashReporting] instance which is used to submit error-level log messages as `RustErrorException`s.

* **feature-push**
  * Fixed a bug where we do not verify subscriptions on first attempt.

* **feature-prompts**
  * Select a regular tab first if that exists before checking custom tabs.

* **feature-session**
  * ⚠️ **This is a breaking change**: `SettingsUseCases` now requires an engine reference, to clear speculative sessions if engine settings change.
  * ⚠️ **This is a breaking change**: `PictureInPictureFeature` now takes `BrowserStore` instead of `SessionManager`.
  * ⚠️ **This is a breaking change**: The `pipChanged` callback has been removed. You should now observe `ContentState.pictureInPictureEnabled` instead.

* **feature-containers**
  * Adds a new storage component to store containers (contextual identities) in a Room database and provides the
    necessary APIs to get, add and remove containers.

* **feature-webnotifications**
  * Adds support for displaying the `source` URL the WebNotification originated from.

* **feature-tabs**
  * ⚠️ **This is a breaking change**: `TabsFeature` now supports providing custom use case implementations. Therefore, an instance of `SelectTabUseCase` and `RemoveTabUseCase` have to be provided.

* **support-ktx**
  * Added `Uri.sameHostAs` to check if two Uris have the same host (both http/https, both same domain).
  * Added `Uri.sameOriginAs` to check if two Uris have the same origin (same host, same port).
  * Added `Uri.isInScope` to check if a Uri is within one of the given scopes.

* **browser-state**
  * Added `ContentState.pictureInPictureEnabled` to track if Picture in Picture mode is in use.

* **feature-pwa**
  * ⚠️ **This is a breaking change**: `WebAppHideToolbarFeature` now takes `BrowserStore` instead of `SessionManager`. `trustedScopes` is now derived from `CustomTabsServiceStore` and `WebAppManifest`. `setToolbarVisibility` should now be used set the visibility of the toolbar.
  * ⚠️ **This is a breaking change**: `onToolbarVisibilityChange` has been removed. You should now observe `BrowserStore` instead.

* **feature-tab-collections**:
  * ⚠️ **This is a breaking change**: `TabCollectionStorage.getCollections` now returns `Flow` instead of `LiveData`. Use `Flow.asLiveData` to convert the result into a `LiveData` again.

* **feature-top-sites**:
  * ⚠️ **This is a breaking change**: `TopSiteStorage.getTopSites` now returns `Flow` instead of `LiveData`. Use `Flow.asLiveData` to convert the result into a `LiveData` again.

* **service-glean**
  * Glean was updated to v31.1.1
    * Smaller binary library after a big dependency was dropped
    * Limit the number of upload retries in all implementations

* **support-images**:
  * Added `ImageLoader` API for loading images directly into an `ImageView`.

* **browser-tabstray**:
  * ⚠️ **This is a breaking change**: `TabsAdapter` and `DefaultTabViewHolder` take an optional `ImageLoader` for loading browser thumbnails.
  * Fixed a bug in `TabsThumbnailView` where the `scaleFactor` was not applied all the time when expected.

* **browser-menu**:
  * DynamicWidthRecyclerView will still be able to have a dynamic width between xml set minWidth and maxWidth attributes but we'll now enforce the following:
    - minimum width 112 dp
    - maximum width - screen width minus a 48dp tappable “exit area”

# 45.0.0

* ⚠️ This release can't be used due to a bad build.

# 44.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v43.0.0...v44.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/104?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v44.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v44.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v44.0.0/buildSrc/src/main/java/Config.kt)

* **browser-engine-gecko-nightly**
  * Added support for [onbeforeunload prompt](https://developer.mozilla.org/en-US/docs/Web/API/WindowEventHandlers/onbeforeunload)

* **feature-tabs**
  * Added an optional `ThumbnailsUseCases` to `TabsFeature` and `TabsTrayPresenter` for loading a
    tab's thumbnail.

* **browser-thumbnails**
  * Adds `LoadThumbnailUseCase` in `ThumbnailsUseCases` for loading the thumbnail of a tab.
  * Adds `ThumbnailStorage` as a storage layer for handling saving and loading a thumbnail from the
    disk cache.

* **feature-push**
  * Adds the `getSubscription` call to check if a subscription exists.

* **browser-engine-gecko-***
  * Fixes GeckoWebPushDelegate to gracefully return when a subscription is not available.

* **feature-session**
  * Removes unused `ThumbnailsFeature` since this has been refactored into its own browser-thumbnails component in
    [#6827](https://github.com/mozilla-mobile/android-components/issues/6827).

* **browser-state**
  * Adds `BrowserState.getNormalOrPrivateTabs(private: Boolean)` to get `normalTabs` or `privateTabs` based on a boolean condition.

* **support-utils**
  * `URLStringUtils.isURLLikeStrict`, deprecated in 40.0.0, was now removed due to performance issues. Use the less strict and much faster `isURLLike` instead or customize based on `:lib-publicsuffixlist`.

* **support-ktx**
  * `String.isUrlStrict`, deprecated in 40.0.0, was now removed due to performance issues. Use the less strict `isURL` instead or customize based on `:lib-publicsuffixlist`.

* **service-glean**
  * Glean was updated to v31.0.2
    * Provide a new upload mechanism, now driven by internals. This has no impact to consumers of service-glean.
    * Automatically Gzip-compress ping payloads before upload
    * Upgrade `glean_parser` to v1.22.0

# 43.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v42.0.0...v43.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/103?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v43.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v43.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v43.0.0/buildSrc/src/main/java/Config.kt)

* **feature-downloads**
  * ⚠️ **This is a breaking change**: DownloadManager and DownloadService are now using the browser store to keep track of queued downloads. Therefore, an instance of the store needs to be provided when constructing manager and service. There's also a new DownloadMiddleware which needs to be provided to the store.

  ```kotlin
   val store by lazy {
        BrowserStore(middleware = listOf(
            MediaMiddleware(applicationContext, MediaService::class.java),
            DownloadMiddleware(applicationContext, DownloadService::class.java),
            ...
        ))
    }
  )

  val feature = DownloadsFeature(
      requireContext().applicationContext,
      store = components.store,
      useCases = components.downloadsUseCases,
      fragmentManager = childFragmentManager,
      onDownloadStopped = { download, id, status ->
          Logger.debug("Download done. ID#$id $download with status $status")
      },
      downloadManager = FetchDownloadManager(
          requireContext().applicationContext,
          components.store, // Store needs to be provided now
          DownloadService::class
      ),
      tabId = sessionId,
      onNeedToRequestPermissions = { permissions ->
          requestPermissions(permissions, REQUEST_CODE_DOWNLOAD_PERMISSIONS)
      }
  )

  class DownloadService : AbstractFetchDownloadService() {
    override val httpClient by lazy { components.core.client }
    override val store: BrowserStore by lazy { components.core.store } // Store needs to be provided now
  }
  ```

  * Fixed issue [#6893](https://github.com/mozilla-mobile/android-components/issues/6893).
  * Add notification grouping to downloads Fenix issue [#4910](https://github.com/mozilla-mobile/android-components/issues/4910).

* **feature-tabs**
  * Makes `TabsAdapter` open to subclassing.

* **feature-intent**
  * Select existing tab by url when trying to open a new tab in `TabIntentProcessor`

* **feature-media**
  * Adds `MediaFullscreenOrientationFeature` to autorotate activity while in fullscreen based on media aspect ratio.

* **support-images**
  * ⚠️ **This is a breaking change**: Extracts `AndroidIconDecoder`, `IconDecoder` and `DesiredSize` out of `browser-icons`
    into a new component `support-images`, which provides helpers for handling images. `AndroidIconDecoder` and `IconDecoder`
    are renamed to `AndroidImageDecoder` and `ImageDecoder` in `support-images`.

* **support-utils**
  * `URLStringUtils.isURLLike()` will now consider URLs containing double dash ("--") as valid.

* **browser-thumbnails**
  * Adds `ThumbnailDiskCache` for storing and restoring thumbnail bitmaps into a disk cache.

* **concept-engine**
  * Adds `onHistoryStateChanged` method and corresponding `HistoryItem` data class.

* **browser-state**
  * Adds `history` to `ContentState` to check the back and forward history list.

* **service-glean**
  * BUGFIX: Fix a race condition that leads to a `ConcurrentModificationException`. [Bug 1635865](https://bugzilla.mozilla.org/1635865)

* **browser-menu**
  * Added `AbstractParentBrowserMenuItem` and `ParentBrowserMenuItem` for handling nested sub menu items on view click.
  * ⚠️ **This is a breaking change**: `WebExtensionBrowserMenuBuilder` now returns as a sub menu entry for add-ons. The sub
    menu also contains an access entry for Add-ons Manager, for which `onAddonsManagerTapped` needs to be passed in the
    constructor.

* **feature-syncedtabs**
  * When the SyncedTabsFeature is started it syncs the devices and account first.

# 42.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v41.0.0...42.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/102?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/42.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/42.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/42.0.0/buildSrc/src/main/java/Config.kt)

* **browser-state**
  * Adds `firstContentfulPaint` to `ContentState` to know if first contentful paint has happened.

* **service-glean**
  * ⚠️ **This is a breaking change**: Glean's configuration now requires explicitly setting an http client. Users need to pass one at construction.
    A default `lib-fetch-httpurlconnection` implementation is available.
    Add it to the configuration object like this:
    `val config = Configuration(httpClient = ConceptFetchHttpUploader(lazy { HttpURLConnectionClient() as Client }))`.
    See [PR #6875](https://github.com/mozilla-mobile/android-components/pull/6875) for details and full code examples.

* **browser-store**
  * Added `webAppManifest` property to `ContentState`.

 * **feature-qr**
   * Added `CustomViewFinder`, a `View` that shows a ViewFinder positioned in center of the camera view and draws an Overlay
   * Added optional String resource `scanMessage` param to `QrFeature` for adding a message below the viewfinder

* **service-experiments**
  * ⚠️ **This is a breaking change**: Mako's configuration now requires explicitly setting an http client. Users need to pass one at construction.

* **feature-prompts**
  * Added `mozacPromptLoginEditTextCursorColor` attribute to be able to change cursor color of TextInputEditTexts from `mozac_feature_prompt_login_prompt`.

# 41.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v40.0.0...v41.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/101?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v41.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v41.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v41.0.0/buildSrc/src/main/java/Config.kt)

* **feature-tabs**
  * Fixed issue [#6907](https://github.com/mozilla-mobile/android-components/issues/6907). Uses MediaState when mapping BrowserState to tabs.

* **concept-tabstray**
  * For issue [#6907](https://github.com/mozilla-mobile/android-components/issues/6907). Adds `Media.State` to `Tab`

* **feature-session**
  * ⚠️ **This is a breaking change**: Added optional `crashReporting` param to [PictureInPictureFeature] so we can record caught exceptions.

* **feature-downloads**
  * Fixed issue [#6881](https://github.com/mozilla-mobile/android-components/issues/6881).

* **feature-addons**
  * Added optional `addonAllowPrivateBrowsingLabelDrawableRes` DrawableRes parameter to `AddonPermissionsAdapter.Style` constructor to allow the clients to add their own drawable. This is used to clearly label the WebExtensions that run in private browsing.

* **browser-menu**
  * BrowserMenu will now support dynamic width based on two new attributes: `mozac_browser_menu_width_min` and `mozac_browser_menu_width_max`.

* **browser-tabstray**
  * Added optional `itemDecoration` DividerItemDecoration parameter to `BrowserTabsTray` constructor to allow the clients to add their own dividers. This is used to ensure setting divider item decoration after setAdapter() is called.

* **service-glean**
  * Glean was updated to v29.1.0
    * ⚠️ **This is a breaking change**: glinter errors found during code generation will now return an error code.
    * The minimum and maximum values of a timing distribution can now be controlled by the `time_unit` parameter. See [bug 1630997](https://bugzilla.mozilla.org/show_bug.cgi?id=1630997) for more details.

* **feature-accounts**
  *  ⚠️ **This is a breaking change**: Refactored component to use `browser-state` instead of `browser-session`. The `FxaWebChannelFeature`  now requires a `BrowserStore` instance instead of a `SessionManager`.

* **lib-push-fcm**, **lib-push-adm**, **concept-push**
  * Allow nullable encoding values in push messsages. If they are null, we attempt to use `aes128gcm` for encoding.

* **browser-toolbar**
  * It will only be animated for vertical scrolls inside the EngineView. Not for horizontal scrolls. Not for zoom gestures.

* **browser-thumbnails**
  * ⚠️ **This is a breaking change**: Migrated this component to use `browser-state` instead of `browser-session`. It is now required to pass a `BrowserStore` instance (instead of `SessionManager`) to `BrowserThumnails`.

# 40.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v39.0.0...v40.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/100?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v40.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v40.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v40.0.0/buildSrc/src/main/java/Config.kt)

* **browser-tabstray**
  * Converts `TabViewHolder` to an abstract class with `DefaultTabViewHolder` as the default implementation.
  * Replaces `layoutId` with `viewHolderProvider` in the `TabsAdapter` for allowing tab customization.

* **concept-engine**
  * Adds Fingerprinter to the recommended Tracking protection policy.

* **support-locale**
  * Adds Android 8.1 to the check in `setLayoutDirectionIfNeeded` inside `LocaleAwareAppCompatActivity`

* **feature-top-sites**
  * ⚠️ **This is a breaking change**: Added `isDefault` to the top site entity, which allows application to specify a default top site that is added by the application. This is called through `TopSiteStorage.addTopSite`.
    * If your application is using Nightly Snapshots of v40.0.0, please test that the Top Sites feature still works and update to the latest v40.0.0 if any schema errors are encountered.

* **feature-push**
  * Simplified error handling and reduced non-fatal exception reporting.

* **support-base**
  * ⚠️ **This is a breaking change**: `CrashReporting` allowing adding support for `recordCrashBreadcrumb` without `lib-crash` dependency.
  * ⚠️ **This is a breaking change**: `Breadcrumb` has moved from `lib-crash` to this component.

* **support-utils**
  * `URLStringUtils.isURLLikeStrict` is now deprecated due to performance issues. Consider using the less strict `isURLLike` instead or creating a new method using `:lib-publicsuffixlist`.

* **support-ktx**
  * `String.isUrlStrict` is now deprecated due to performance issues. Consider using the less strict `isURL` instead or creating a new method using `:lib-publicsuffixlist`.

* **browser-menu**
  * Added `SimpleBrowserMenuHighlightableItem` which is a simple menu highlightable item (no images/icons).

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * Improve social trackers categorization see [ac#6851](https://github.com/mozilla-mobile/android-components/issues/6851) and [fenix#5921](https://github.com/mozilla-mobile/fenix/issues/5921)

* **service-firefox-accounts**
  * ⚠️ **This is a breaking change**: `FxaAccountManager.withConstellation` puts the `DeviceConstellation` within the same scope as the block so you no longer need to use the `it` reference.

* **feature-syncedtabs**
  * Moved `SyncedTabsFeature` to `SyncedTabsStorage`.
  * ⚠️ **This is a breaking change**: The new `SyncedTabsFeature` now orchestrates the correct state needed for consumers to handle by implementing the `SyncedTabsView`.

* **browser-thumbnails**
  * 🆕 New component for capturing browser thumbnails.
  * `ThumbnailsFeature` will be deprecated for the new `BrowserThumbnails` in a future.

# 39.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v38.0.0...v39.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/99?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v39.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v39.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v39.0.0/buildSrc/src/main/java/Config.kt)

* **All components**
  * Increased `targetSdkVersion` to 29 (Android Q)

* **browser-session**
  * `SnapshotSerializer` no longer restores source of a `Session`, added `Session.Source.RESTORED`

* **feature-downloads**
  * Fixed issue [#6764](https://github.com/mozilla-mobile/android-components/issues/6764).

* **support-locale**
  * Added fix for respecting RTL/LTR changes when activity is recreated in Android 8

* **feature-sitepermissions**
  * ⚠️ **This is a breaking change**: The `SitePermissionsFeature`'s constructor, now requires a new parameter `onShouldShowRequestPermissionRationale` a lambda to allow the feature to query [ActivityCompat.shouldShowRequestPermissionRationale](https://developer.android.com/reference/androidx/core/app/ActivityCompat#shouldShowRequestPermissionRationale(android.app.Activity,%20java.lang.String)) or [Fragment.shouldShowRequestPermissionRationale](https://developer.android.com/reference/androidx/fragment/app/Fragment#shouldShowRequestPermissionRationale(java.lang.String)). This allows the `SitePermissionsFeature` to handle when a user clicks "Deny & don't ask again" button in a system permission dialog, for more information see [issue #6565](https://github.com/mozilla-mobile/android-components/issues/6565).

* **ui-widgets**
  * 🆕 New component for standardized Mozilla widgets and styles. A living style guide will be published soon that helps explain design choices made.
  * First version includes styling for buttons: `NeutralButton`, `PositiveButton`, and `DestructiveButton`

* **feature-addons**
  * Added `AddonsManagerAdapter.updateAddon` and `AddonsManagerAdapter.updateAddons` to allow partial updates.
  * ⚠️ **This is a breaking change**: `AddonsManagerAdapterDelegate.onNotYetSupportedSectionClicked(unsupportedAddons: ArrayList<Addon>)` is changed to `AddonsManagerAdapterDelegate.onNotYetSupportedSectionClicked(unsupportedAddons: List<Addon>)`.
  * Fixed [issue #6685](https://github.com/mozilla-mobile/android-components/issues/6685), now `DefaultSupportedAddonsChecker` will marked any newly supported add-on as enabled.
  * Added `Addon.translatedSummary` and `Addon.translatedDescription` to ease add-on translations.
  * Added `Addon.defaultLocale` Indicates which locale will be always available to display translatable fields.
  * ⚠️ **This is a breaking change**: `AddonManager.enableAddon` and `AddonManager.disableAddon` have a new optional parameter  `source` that indicates why the extension is enabled/disabled.
  * ⚠️ **This is a breaking change**: `Map<String, String>.translate` now is marked as internal, if you are trying to translate the summary or the description of an add-on, use `Addon.translatedSummary` and `Addon.translatedDescription`.

* **feature-toolbar**
  * Disabled autocompleting when updating url upon entering edit mode in BrowserToolbar.

* **feature-media**
  * Muted media will not start the media service anymore, causing no media notification to be shown and no audio focus getting requested.

* **feature-fullscreen**
  * ⚠️ **This is a breaking change**: Added `viewportFitChanged` to support Android display cutouts.

  * **feature-qr**
    * Moved `AutoFitTextureView` from `support-base` to `feature-qr`.

* **feature-session**
  * ⚠️ **This is a breaking change**: Added `viewportFitChanged` to `FullScreenFeature` for supporting Android display cutouts.

* **feature-qr**
  * Moved `AutoFitTextureView` from `support-base` to `feature-qr`.

* **service-sync-logins**
  * Adds fun `LoginsStorage.getPotentialDupesIgnoringUsername` for fetching list of potential duplicate logins from the underlying storage layer, ignoring username.

* **feature-customtabs**
  * ⚠️ **This is a breaking change**: Removed `handleError` in `CustomTabWindowFeature` constructor
  * ⚠️ **This is a breaking change**: Added `onLaunchUrlFallback` to `CustomTabWindowFeature` constructor

* **browser-tabstray**
  * The iconView is no longer required in the template.
  * The URL text for items may be styled.

* **service-glean**
  * Glean was updated to v28.0.0
    * The baseline ping is now sent when the application goes to foreground, in addition to background and dirty-startup.

* **Developer ergonomics**
  * Improved autoPublication workflow. See https://mozac.org/contributing/testing-components-inside-app for updated documentation.

* **browser-search**
  * Added `getSearchTemplate` to reconstruct the user-entered search engine url template

# 38.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v37.0.0...v38.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/97?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v38.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v38.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v38.0.0/buildSrc/src/main/java/Config.kt)

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * **Merge day!**
    * `browser-engine-gecko-release`: GeckoView 75.0
    * `browser-engine-gecko-beta`: GeckoView 76.0
    * `browser-engine-gecko-nightly`: GeckoView 77.0

* **feature-session**
  * ⚠️ **This is a breaking change**: Added optional `customTabSessionId` param to [PictureInPictureFeature] so consumers can use this feature for custom tab sessions.

* **support-locale**
  * Updates `updateResources` to always update the context configuration

* **feature-toolbar**
  * Added `forceExpand` to [BrowserToolbarBottomBehavior] so consumers can expand the BrowserToolbar on demand.

* **feature-addons**
  * Added `AddonPermissionsAdapter.Style` and `AddonsManagerAdapter.Style` classes to allow UI customization.

* **service-accounts-push**
  * Fixed a bug where the push subscription was incorrectly cached and caused some `GeneralError`s.
* **feature-addons**
  * Added `DefaultAddonUpdater.UpdateAttemptStorage` allows to query the last known state for a previous attempt to update an add-on.

* **feature-accounts-push**
  * Re-subscribe for Sync push support when notified by `onSubscriptionChanged` events.

* **support-migration**
  * ⚠️ **This is a breaking change**: `FennecMigrator` now takes `Lazy` references to storage layers.

* **concept-storage**, **service-sync-logins**
  * 🆕 New API: `PlacesStorage#warmUp`, `SyncableLoginsStorage#warmUp` - allows consumers to ensure that underlying storage database connections are fully established.

* **feature-customtabs**
  * ⚠️ **This is a breaking change**: add parameter `handleError` to `CustomTabWindowFeature` constructor
    * This is used to show an error when the url can't be handled
  * `CustomTabIntentProcessor` to support `Browser.EXTRA_HEADERS`.

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * Fixed a memory leak when using a `SelectionActionDelegate` on `GeckoEngineView`.

* **feature-share**
  * Added `RecentAppsStorage.deleteRecentApp` and `RecentAppsDao.deleteRecentApp` to allow deleting a `RecentAppEntity`

* **feature-p2p**
  * Add new `P2PFeature` to send URLs and web pages through peer-to-peer networking.

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly, **browser-engine-system**
  * Added `GeckoEngineView#getInputResult()` to return an EngineView.InputResult indicating how a MotionEvent was handled by an EngineView.

* **concept-engine**
  * Will expose a new `InputResult` enum through `getInputResult()` indicating how an EngineView handled user's MotionEvent.
  * See above changes to browser-engine-*.

* **browser-toolbar**
  * `BrowserToolbarBottomBehavior` is now solely responsible to decide if the dynamic nav bar should animate or not.
  * See above changes to browser-engine-*, concept-engine.

* **feature-session**
  * `SwipeRefreshLayout` will now trigger pull down to refresh only if the website is scrolled to top and it itself did not consume the swype event.
  * See above changes to browser-engine-*, concept-engine.
  * Added androidx_swiperefreshlayout as a dependency because google_materials dependency was incremented to version 1.1.0 which no longer includes SwipeRefreshLayout

* **lib-crash**
  * ⚠️ **This is a breaking change**: added `support-base` dependency.

* **support-base**
  * `CrashReporting` allowing adding support for `submitCaughtException` without `lib-crash` dependency.

* **browser-tabstray**
 * Added ability to let consumers pass a custom layout of `TabViewHolder` in order to control layout inflation and view binding.
 * Added an optional URL view to the `TabViewHolder` to display the URL.
 * Will expose a new `layout` parameter which allows consumers to change the tabs tray layout.
 * Will only display a URL's hostname instead of the entire URL

* **browser-session**
  * ⚠️ **This is a breaking change**: `SessionManager.runWithSessionIdOrSelected` now returns the result from the `block` it executes. This is consistent with `runWithSession`.

* **feature-push**
  * Allow nullable AutoPush messages to be delivered to observers.

* **service-glean**
  * Glean was updated to v27.1.0
    * **Breaking change:** The regular expression used to validate labels is stricter and more correct.
    * BUGFIX: baseline pings sent at startup with the `dirty_startup` reason will now include application lifetime metrics ([#810](https://github.com/mozilla/glean/pull/810))
    * Add more information about pings to markdown documentation:
      * State whether the ping includes client id;
      * Add list of data review links;
      * Add list of related bugs links.
    * `gradlew clean` will no longer remove the Miniconda installation in `~/.gradle/glean`. Therefore `clean` can be used without reinstalling Miniconda afterward every time.
    * Glean will now detect when the upload enabled flag changes outside of the application, for example due to a change in a config file. This means that if upload is disabled while the application wasn't running (e.g. between the runs of a Python command using the Glean SDK), the database is correctly cleared and a deletion request ping is sent. See [#791](https://github.com/mozilla/glean/pull/791).
    * The `events` ping now includes a reason code: `startup`, `background` or `max_capacity`.

# 37.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v36.0.0...v37.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/96?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v37.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v37.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/v37.0.0/master/buildSrc/src/main/java/Config.kt)

* **lib-state**, **browser-state**
  * Added the ability to add `Middleware` instances to a `Store`. A `Middleware` can rewrite or intercept an `Action`, as well as dispatch additional `Action`s or perform side-effects when an `Action` gets dispatched.

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * Updated `removeAll()` from `TrackingProtectionExceptionFileStorage` to notify active sessions when all exceptions are removed.
  * Added `GeckoPort.senderUrl` which returns the associated content URL.

* **feature-accounts**
  * It should now be possible to log-in on stable, stage and china FxA servers using a WebChannel flow.
  *  ⚠️ **This is a breaking change**: `FxaWebChannelFeature` takes a `ServerConfig` as 4th parameter to ensure incoming WebChannel messages
     are sent by the expected FxA host.

* **feature-sitepermissions**
  * Fixed issue [#6299](https://github.com/mozilla-mobile/android-components/issues/6299), from now on, any media requests like a microphone or a camera permission will require the system permissions to be granted before a dialog can be shown.

* **service-sync-logins**
  * ⚠️ **This is a breaking change**: `DefaultLoginValidationDelegate`, `GeckoLoginStorageDelegate` constructors changed to take `Lazy<LoginsStorage>` instead of `LoginsStorage`, to facilitate late initialization.

* **service-firefox-accounts**
  * ⚠️ **This is a breaking change**: `GlobalSyncableStoreProvider#configureStore` changed to take `Lazy<SyncableStore>` instead of `SyncableStore`, to facilitate late initialization.

* **feature-session**
  * ⚠️ **This is a breaking change**: `HistoryDelegate` constructor changed to take `Lazy<HistoryStorage>` instead of `HistoryStorage`, to facilitate late initialization.

* **concept-engine**
  * Added: `DataCleanable` a new interface that decouples the behavior of clearing browser data from the `Engine` and `EngineSession`.

* **feature-sitepermissions**
  *  Fixed [#6322](https://github.com/mozilla-mobile/android-components/issues/6322), now  `SitePermissionsStorage` allows to indicate an optional reference to `DataCleanable`.

* **browser-menu**
  * Added `canPropagate` param to all `BrowserMenuHighlight`s, making it optional to be displayed in other components
  * Changed `BrowserMenuItem.getHighlight` to filter the highlights which should not propagate.

* **feature-share**
    * Changed primary key of RecentAppEntity to activityName instead of packageName
      * ⚠️ **This is a breaking change**: all calls to app.packageName should now use app.activityName

# 36.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v35.0.0...v36.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/95?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v36.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v36.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v36.0.0/buildSrc/src/main/java/Config.kt)

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * **Merge day!**
    * `browser-engine-gecko-release`: GeckoView 74.0
    * `browser-engine-gecko-beta`: GeckoView 75.0
    * `browser-engine-gecko-nightly`: GeckoView 76.0

* **feature-addons**
  * Added `DefaultSupportedAddonsChecker` which checks for add-ons that were previously unsupported, and creates a notification to let the user known when they are available to be used.

* **feature-push**
  * `AutoPushFeature` now properly notifies observers that they have changed by the `Observer.onSubscriptionChanged` callback.
  *  ⚠️ **This is a breaking change**: `RustPushConnection.verifyConnection` now returns a list of subscriptions that have changed.

# 35.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v34.0.0...v35.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/94?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v35.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v35.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v35.0.0/buildSrc/src/main/java/Config.kt)

* **feature-sitepermissions**
  *  Fixed [#5616](https://github.com/mozilla-mobile/android-components/issues/5616) issue now when a new exception is added if the [sitePermissionsRules](https://github.com/mozilla-mobile/android-components/blob/52de7118a706a88e88036ead6192e042d4423dc6/components/feature/sitepermissions/src/main/java/mozilla/components/feature/sitepermissions/SitePermissionsFeature.kt#L72) object is present its values are going to be taken into consideration as default values for the new exception.

* **feature-awesomebar**
  *  ⚠️ **This is a breaking change**: Refactored component to use `browser-state` instead of `browser-session`. Feature and `SuggestionProvider` implementations may require a `BrowserStore` instance instead of a `SessionManager` now.

* **feature-intent**
  * ⚠️ **This is a breaking change**: Removed `IntentProcessor.matches()` method from interface. Calling `process()` and examining the boolean return value is enough to know whether an `Intent` matched.

* **feature-downloads**
  * Fixed APK downloads not prompting to install when the notification is clicked.

* **service-location**
  * Created `LocationService` interface and made `MozillaLocationService` implement it.
  * `RegionSearchLocalizationProvider` now accepts any `LocationService` implementation.
  * Added `LocationService.dummy()` which creates a dummy `LocationService` implementation that always returns `null` when asked for a `LocationService.Region`.

* **feature-accounts-push**
  * Add known prefix to FxA push scope.

* **browser-toolbar**
  * Add the possibility to listen to menu dismissal through `setMenuDismissAction` in `DisplayToolbar`

* **concept-storage**
  * New interface: `LoginsStorage`, describes a logins storage. A slightly cleaned-up version of what was in the `service-sync-logins`.

* **service-sync-logins**
  * ⚠️ **This is a breaking change**: Refactored `AsyncLoginsStorage`, which is now called `SyncableLoginsStorage`. New class caches the db connection, and removes lock/unlock operations from the public API.

* **feature-tabs**
  * Fixed close tabs callback incorrectly invoked on start.
  * ⚠️ **This is a breaking change**: Added `defaultTabsFilter` to `TabsFeature` for initial presenting of tabs.
    * `TabsFeature.filterTabs` also uses the same filter if no new filter is provided.

* **browser-session**
  * SessionManager will now close internal `EngineSession` instances on memory pressure when `onTrimMemory()` gets called
  * `SessionManager.onLowMemory()` is now deprecated and `SessionManager.onTrimMemory(level)` should be used instead.

* **browser-engine-system**
  * Updated tracking protection lists for more details see [#6163](https://github.com/mozilla-mobile/android-components/issues/6163).

* **concent-engine**, **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**, **browser-engine-system**
  * Add additional HTTP header support for `EngineSession.loadUrl()`.

# 34.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v33.0.0...v34.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/93?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v34.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v34.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v34.0.0/buildSrc/src/main/java/Config.kt)

* **concept-tabstray**
  * ⚠️ **This is a breaking change**: Removed dependency on `browser-session` and introduced tabs tray specific data classes.

* **browser-tabstray**
  * ⚠️ **This is a breaking change**: Refactored component to implement updated `concept-tabstray` interfaces.

* **feature-tabs**
  * ⚠️ **This is a breaking change**: Refactored component to use `browser-state` instead of `browser-session`.
  * Added additional method to `TabsUseCases` to select a tab based on its id.

* **feature-contextmenu**
  * Adds optional `shareTextClicked` lambda to `DefaultSelectionActionDelegate` which allows adding and dispatching a text selection share action

* **browser-icons**
  * ⚠️ **This is a breaking change**: Migrated this component to use `browser-state` instead of `browser-session`. It is now required to pass a `BrowserStore` instance (instead of `SessionManager`) to `BrowserIcons.install()`.

* **support-webextensions**
  * Emit facts on installed and enabled addon ids after web extension is initialized.

* **feature-app-links**
  * ⚠️ **This is a breaking change**: `alwaysAllowedSchemes` is removed as a parameter for `AppLinksInterceptor`.
  * Added `engineSupportedSchemes` as a parameter for `AppLinksInterceptor`.  This allows the caller to specify which protocol is supported by the engine.
    * Using this information, app links can decide if a protocol should be launched in a third party app or not regardless of user preference.

* **feature-sitepermissions**
  * ⚠️ **This is a breaking change**: add parameters `autoplayAudible` and `autoplayInaudible` to `SitePermissionsRules`.
    * This allows autoplay settings to be controlled for specific sites, rather than globally.

* **concept-engine**
  * ⚠️ **This is a breaking change**: remove deprecated GeckoView setting `allowAutoplayMedia`
    * This should now be controlled for individual sites via `SitePermissionsRules`
  * Fixed a bug that would cause `TrackingProtectionPolicyForSessionTypes` to lose some information during transformations.

* **feature-downloads**
  * ⚠️ **This is a breaking change**: `customTabId` is renamed to `tabId`.

* **feature-contextmenu**
  * ⚠️ **This is a breaking change**: `customTabId` is renamed to `tabId`.

* **browser-menu**
  * Emit fact on the web extension id when a web extension menu item is clicked.

* **feature-push**
  * ⚠️ **This is a breaking change**:
    * Removed APIs from AutoPushFeature: `subscribeForType`, `unsubscribeForType`, `subscribeAll`.
    * Removed `PushType` enum and it's internal uses.
    * Use the new ✨ `subscribe` and `unsubscribe` APIs.

* **feature-accounts-push**
  * Updated `FxaPushSupportFeature` to use the new `AutoPushFeature` APIs.

* **concept-sync**
 * ⚠️ **This is a breaking change**:
  * `DeviceEvent` and related classes were expanded/refactored, and renamed to `AccountEvent`.
  * `DeviceConstellation` "event" related APIs were renamed to be "command"-centric.

* **service-firefox-accounts**
 * ⚠️ **This is a breaking change**:
  * `FxaAccountManager.registerForDeviceEvents` renamed to `FxaAccountManager.registerForAccountEvents`.

# 33.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v32.0.0...v33.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/93?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v33.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v33.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v33.0.0/buildSrc/src/main/java/Config.kt)

* **feature-share**
  * Added database to store recent apps
  * Added `RecentAppsStorage` to handle storing and retrieving most-recent apps

* **service-glean**
  * Glean was updated to v25.0.0:
    * General:
      * `ping_type` is not included in the `ping_info` any more ([#653](https://github.com/mozilla/glean/pull/653)), the pipeline takes the value from the submission URL.
      * The version of `glean_parser` has been upgraded to 1.18.2:
        * **Breaking Change (Java API)** Have the metrics names in Java match the names in Kotlin.
          See [Bug 1588060](https://bugzilla.mozilla.org/show_bug.cgi?id=1588060).
        * The reasons a ping are sent are now included in the generated markdown documentation.
    * Android:
      * The `Glean.initialize` method runs mostly off the main thread ([#672](https://github.com/mozilla/glean/pull/672)).
      * Labels in labeled metrics now have a correct, and slightly stricter, regular expression.
        See [label format](https://mozilla.github.io/glean/user/metrics/index.html#label-format) for more information.

# 32.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v31.0.0...v32.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/92?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v32.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v32.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v32.0.0/buildSrc/src/main/java/Config.kt)

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * **Merge day!**
    * `browser-engine-gecko-release`: GeckoView 73.0
    * `browser-engine-gecko-beta`: GeckoView 74.0
    * `browser-engine-gecko-nightly`: GeckoView 75.0

* **browser-engine-gecko-nightly**, **concept-engine**
  * Updated `WebPushHandler` and `GeckoWebPushHandler` to accept push scopes for `onSubscriptionChanged` events.

* **WebExtensions refactor**
  * The Web Extensions related methods have been refactored from `Engine` into a new `WebExtensionRuntime` interface.
  * The `Engine` interface now implements the `WebExtensionRuntime` interface.
  * `WebCompatFeature` has been updated to receive a `WebExtensionRuntime` instead of a `Engine` as `install` method parameter.

* **lib-crash**
  * Now supports adding telemetry crash reporting services.  Telemetry services will not require to prompt
  * the user since it is restricted by the telemetry preference of the user.
  ```kotlin
  CrashReporter(
      services = services,
      telemetryServices = telemetryServices,
      shouldPrompt = CrashReporter.Prompt.ALWAYS,
      promptConfiguration = CrashReporter.PromptConfiguration(
          appName = context.getString(R.string.app_name),
          organizationName = "Mozilla"
      ),
      enabled = true,
      nonFatalCrashIntent = pendingIntent
  )
  ```

* **feature-search**
  * Adds `DefaultSelectionActionDelegate`, which may be used to add new actions to text selection context menus.
    * It currently adds "Firefox Search" or "Firefox Private Search", depending on whether the selected tab is private.
  * Adds `SearchFeature`, which consumes search requests made by other components.
  ```kotlin
  // Example usage

  // Attach `DefaultSelectionActionDelegate` to the `EngineView`
  override fun onCreateView(parent: View?, name: String, context: Context, attrs: AttributeSet): View? =
      when (name) {
          EngineView::class.java.name -> components.engine.createView(context, attrs).apply {
              selectionActionDelegate = DefaultSelectionActionDelegate(
                  components.store,
                  context,
                  "My App Name"
              )
          }.asView()
      }

  // Use `SearchFeature` to attach search requests to your own code
  private val searchFeature = ViewBoundFeatureWrapper<SearchFeature>()
  // ...
  searchFeature.set(
      feature = SearchFeature(components.store) {
          when (it.isPrivate) {
              false -> components.searchUseCases.newTabSearch.invoke(it.query)
              true -> components.searchUseCases.newPrivateTabSearch.invoke(it.query)
          }
      },
      owner = this,
      view = layout
  )
  ```

* **service-glean**
  * Glean was updated to v24.2.0:
    * Add `locale` to `client_info` section.
    * **Deprecation Warning** Since `locale` is now in the `client_info` section, the one
      in the baseline ping ([`glean.baseline.locale`](https://github.com/mozilla/glean/blob/c261205d6e84d2ab39c50003a8ffc3bd2b763768/glean-core/metrics.yaml#L28-L42))
      is redundant and will be removed by the end of the quarter.
    * Drop the Glean handle and move state into glean-core ([#664](https://github.com/mozilla/glean/pull/664))
    * If an experiment includes no `extra` fields, it will no longer include `{"extra": null}` in the JSON payload.
    * Support for ping `reason` codes was added.
      * The metrics ping will now include `reason` codes that indicate why it was
        submitted.
      * The baseline ping will now include `reason` codes that indicate why it was
        submitted. If an unclean shutdown is detected (e.g. due to force-close), this
        ping will be sent at startup with `reason: dirty_startup`.
    * The version of `glean_parser` has been upgraded to 1.17.3
    * Collections performed before initialization (preinit tasks) are now dispatched off
      the main thread during initialization.

* **feature-awesomebar**
  * Added `showDescription` parameter (default to `true`) to `SearchSuggestionProvider` constructors to add the possibility of removing search suggestion description.

* **support-migration**
  * Emit facts during migration.

# 31.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v30.0.0...v31.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/91?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v31.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v31.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v31.0.0/buildSrc/src/main/java/Config.kt)

* **feature-awesomebar**
  * ⚠️ **This is a breaking change**: Added resources parameter to `addSessionProvider` method from `AwesomeBarFeature` and to `SessionSuggestionProvider` constructor for accessing strings.


# 30.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v29.0.0...v30.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/90?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v30.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v30.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/mv30.0.0aster/buildSrc/src/main/java/Config.kt)


# 29.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v28.0.0...v29.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/89?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v29.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v29.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v29.0.0/buildSrc/src/main/java/Config.kt)

* **feature-error-pages**
  * ⚠️ **This is a breaking change**: ErrorResponse now has two data classes: Content (for data URI's) and Uri (for encoded URL's)
    * This will require a change in RequestInterceptors that override `onErrorRequest`.
    * Return the corresponding ErrorResponse (ErrorResponse.Content or ErrorResponse.Uri) as ErrorResponse can no longer be directly instantiated.
  * Added support for loading images into error pages with `createUrlEncodedErrorPage`. These error pages load dynamically with javascript by parsing params in the URL
  * ⚠️ To use custom HTML & CSS with image error pages, resources **must** be located in the assets folder

* **feature-prompts**
  * Save login prompts will no longer be closed on page load

* **lib-crash**
  * Glean reports now distinguishes between fatal and non-fatal native code crashes.

* **feature-pwa**
  * Added ability to query install state of an url.
  * Added ability load all manifests that apply to a certain url.
  * Added ability to track if an PWA is actively used.

# 28.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v27.0.0...v28.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/88?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v28.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v28.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v28.0.0/buildSrc/src/main/java/Config.kt)

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * **Merge day!**
    * `browser-engine-gecko-release`: GeckoView 72.0
    * `browser-engine-gecko-beta`: GeckoView 73.0
    * `browser-engine-gecko-nightly`: GeckoView 74.0

* **feature-session**
  * * ⚠️ **This is a breaking change**: `TrackingProtectionUseCases.fetchExceptions`: now receives a `(List<TrackingProtectionException>) -> Unit` instead of a `(List<String>) -> Unit` to add support for deleting individual exceptions.
    * **Added**: `TrackingProtectionUseCases.removeException(exception: TrackingProtectionException)`: now you can delete an exception without the need of having a `Session` by calling `removeException(trackingProtectionException)`.

* **service-glean**
  * Glean was updated to v24.1.0:
    * **Breaking Change** An `enableUpload` parameter has been added to the `initialize()`
      function. This removes the requirement to call `setUploadEnabled()` prior to calling
      the `initialize()` function.
    * A new metric `glean.error.preinit_tasks_overflow` was added to report when
      the preinit task queue overruns, leading to data loss. See [bug
      1609482](https://bugzilla.mozilla.org/show_bug.cgi?id=1609482)
    * The metrics ping scheduler will now only send metrics pings while the
      application is running. The application will no longer "wake up" at 4am
      using the Work Manager.
    * The code for migrating data from Glean SDK before version 19 was removed.
    * When using the `GleanTestLocalServer` rule in instrumented tests, pings are
      immediately flushed by the `WorkManager` and will reach the test endpoint as
      soon as possible.
    * The Glean Gradle Plugin correctly triggers docs and API updates when registry files
      change, without requiring them to be deleted.
    * `parseISOTimeString` has been made 4x faster. This had an impact on Glean
      migration and initialization.
    * Metrics with `lifetime: application` are now cleared when the application is started,
      after startup Glean SDK pings are generated.
    * ⚠️ **This is a breaking change**:
      * The public method `PingType.send()` (in all platforms) have been deprecated
        and renamed to `PingType.submit()`.
    * Rename `deletion_request` ping to `deletion-request` ping after glean_parser update
    * BUGFIX: The Glean Gradle plugin will now work if an app or library doesn't
      have a metrics.yaml or pings.yaml file.

* **feature-app-links**
  * AppLinksInterceptor can now be used without the AppLinksFeature. Set the new parameter launchFromInterceptor = true
  ```kotlin
  AppLinksInterceptor(
      applicationContext,
      interceptLinkClicks = true,
      launchInApp = { true },
      launchFromInterceptor = true
  )
  ```
  * Introduce a `ContextMenuCandidate` to open links in the corresponding external app, if installed

* **concept-storage**
  * Added classes related to login autofill
    * `LoginStorageDelegate` may be attached to an `Engine`, where it can be used to save logins.
    * `LoginValidationDelegate` may be used to read and update currently saved logins.

* **feature-prompts**
  * `PromptFeature` may now optionally accept a `LoginValidationDelegate`. If present, it users
  will be prompted to save their information after logging in to a website.
  * `PromptFeature` now accepts a false by default `isSaveLoginEnabled` lambda to be invoked before showing prompts. If true, users
    will be prompted to save their information after logging in to a website.
  * Prompts will now be closed automatically when pages have mostly loaded

* **service-sync-logins**
  * Added `GeckoLoginStorageDelegate`. This can be attached to a GeckoEngine, where it will be used
  to save user login credentials.
  * `GeckoLoginStorageDelegate` now accepts a false by default `isAutofillEnabled` lambda to be invoked before fetching logins. If false,
   logins will not be fetched to autofill.

* **service-firefox-accounts**
  * `signInWithShareableAccountAsync` now takes a `reuseAccount` parameter, allowing consumers
    to reuse existing session token (and FxA device) associated with the passed-in account.

* **support-migration**
  * **New Telemetry Notice**
  * Added a 'migration' ping, which contains telemetry data about migration via Glean. It's emitted whenever a migration is executed.
  * Added `MigrationIntentProcessor` for handling incoming intents when migration is in progress.
  * Added `AbstractMigrationProgressActivity` as a base activity to block user interactions during migration.

* **browser-menu**
  * Added `MenuButton` to let the browser menu be used outside of `BrowserToolbar`.

* **browser-menu**
  * Added new `MenuController` and `MenuCandidate` items that replace `BrowserMenuBuilder` and `BrowserMenuItem`.
    * Menu candidates are pure data classes and state changes are done by submitting a new list of menu candidates, rather than mutating menu items.
    * The current state of a `BrowserMenuItem` can be converted to a menu candidate with `asCandidate(Context)`.
    * `MenuController` replaces both `BrowserMenuBuilder` and `BrowserMenu`, and handles showing the menu and adjusting the displayed items.
    * `MenuView` lets the menu be used outside of popups.
    * Menu candidates add support for buttons inside menu options, text to the side of a menu option, and improved a11y.
  * ⚠️ **This is a breaking change**: Added `asCandidate` method to `BrowserMenuItem` interface.

* **browser-toolbar**
  * Added support for the new menu controller through the `DisplayToolbar.menuController` property.


# 27.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v26.0.0...v27.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/86?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v27.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v27.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v27.0.0/buildSrc/src/main/java/Config.kt)

* **feature-remotetabs** was renamed to **feature-syncedtabs**
  * ⚠️ **This is a breaking change**:
  * `RemoteTabsFeature` is now `SyncedTabsFeature`, and some method names have corresponding changes.
  * `RemoteTabsStorageSuggestionProvider` is now `SyncedTabsStorageSuggestionProvider`.

* **service-glean**
  * Glean was updated to v22.1.0 ([Full changelog](https://github.com/mozilla/glean/compare/v21.3.0...v22.1.0))
    * Attempt to re-send the deletion ping on init even if upload is disabled.
    * Introduce the `InvalidOverflow` error for `TimingDistribution`s.
  * Glean now provides a Gradle plugin for automating the conversion from `metrics.yaml` and `pings.yaml` files to Kotlin code. This should be used instead of the deprecated Gradle script.  See [integrating with the build system docs](https://mozilla.github.io/glean/book/user/adding-glean-to-your-project.html#integrating-with-the-build-system) for more information.

* **feature-app-links**
  * ⚠️ **This is a breaking change**:
  * Feature now contains two parts.  One part is the AppLinksFeature, the other part is RequestInterceptor.
  ```kotlin
  // add this call in the RequestInterceptor
  context.components.appLinksInterceptor.onLaunchIntentRequest(engineSession, uri, hasUserGesture, isSameDomain)
  ```

* **support-telemetry-sync**
  * Added new telemetry ping, to support password sync: `passwords_sync`.

* **service-firefox-accounts**
  * 🕵️  **New Telemetry Notice**
  * Added telemetry for password sync, via the new `passwords_sync` in **support-telemetry-sync**

* **sync-logins**
  * 🕵️  **New Telemetry Notice**
  * Added telemetry for password sync, via the new `passwords_sync` in **support-telemetry-sync**
  * The `service-sync-logins` component now collects some basic performance and quality metrics via Glean.
    Applications that send telemetry via Glean *must ensure* they have received appropriate data-review before integrating this component.
  * ⚠️ **This is a breaking change**: The `ServerPassword` fields `username`, `usernameField` and `passwordField` can no longer by `null`.
    Use the empty string to indicate an absent value for these fields.
  * ⚠️ **This is a breaking change**: The `AsyncLoginsStorageAdapter.inMemory` method has been removed.
    Use `AsyncLoginsStorageAdapter.forDatabase(":memory:")` instead.


* **samples-sync**
  * Added support for password synchronization (not reflected in the UI, but demonstrates how to integrate the component).

* **browser-menu**
  * Added `BrowserMenuHighlightableSwitch` to represent a highlightable item with a toggle switch.

* **lib-crash**
  * Now supports performing action after submitting crash report.
  ```kotlin
  crashReporter.submitReport(Crash.fromIntent(intent)) {
      stopSelf()
  }
  ```

* **support-ktx**
  * Added `Context.getDrawableWithTint` extension method to get a drawable resource with a tint applied.
	* `String.isUrl` is now using a more lenient check for improved performance. Strictly checking whether a string is a URL or not is supported through the new `String.isUrlStrict` method.

* **support-base**
  * ⚠️ **This is a breaking change**:
  * Removed helper for unique notification id.
  * Added helper for providing unique stable `Int` ids based on a `String` tag to avoid id conflicts between components and app code.  This is now for any id, not just notification id.
  * Added new API that allows user to request for the next available id using the same tag.

  ```kotlin
  // Get a unique id for the provided tag
  val id = SharedIdsHelper.getIdForTag(context, "mozac.my.feature")

  // Get the next unique id for the provided tag
  val id = SharedIdsHelper.getNextIdForTag(context, "mozac.my.feature")
  ```

* **browser-errorpages**
  * Added support for bypassing invalid SSL certification.

# 26.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v25.0.0...v26.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/85?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v26.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v26.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v26.0.0/buildSrc/src/main/java/Config.kt)

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * **Merge day!**
    * `browser-engine-gecko`: GeckoView 71.0
    * `browser-engine-gecko-beta`: GeckoView 72.0
    * `browser-engine-gecko-nightly`: GeckoView 73.0

* **browser-engine-system** and **browser-engine-gecko-nightly**
  * Added `EngineView.canClearSelection()` and `EngineView.clearSelection()` for clearing the selection.

* **feature-accounts**
  * ⚠️ **This is a breaking change**: Migrated `FxaPushSupportFeature` to the `feature-accounts-push` component.

* **feature-sendtab**
  * ⚠️ **This is a breaking change**: This component is now deprecated. See `feature-accounts-push`.

* **feature-accounts-push**
  * 🆕 New component for features that need Firefox Accounts and Push, e.g. Send Tab.
  * `SendTabFeature` and `SendTabUseCases` have been migrated here.
  * ⚠️ **This is a breaking change**: `SendTabFeature` no longer takes an instance of `AutoPushFeature`.
    * `FxaPushSupportFeature` is now needed for integrating Firefox Accounts with Push support.

* **support-test-libstate**
  * 🆕 New component providing utilities to test functionality that relies on lib-state.

* **browser-errorpages**
  * Removed list items semantics to improve a11y for unordered lists, preventing items being read twice.

* **support-locale**
    * Add `resetToSystemDefault` and `getSystemDefault` method to `LocaleManager`

# 25.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v24.0.0...v25.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/84?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v25.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v25.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v25.0.0/buildSrc/src/main/java/Config.kt)

* **feature-downloads**
  * Makes `DownloadState` parcelizable so that it can be passed to `FetchDownloadManager` when completed

* **feature-remotetabs**
  * Add new `RemoteTabsFeature` to view tabs from other synced devices and upload our own.
  * Add `RemoteTabsStorageSuggestionProvider` class to match remote tabs in awesomebar suggestions.

* **support-migration**
  * Added Fennec login migration logic.

* **service-sync-logins**
  * `AsyncLoginsStorage` interface gained a new method: `importLoginsAsync`, used for bulk-inserting logins (for example, during a migration).

# 24.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v23.0.0...v24.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/84?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v24.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v24.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v24.0.0/buildSrc/src/main/java/Config.kt)

* **browser-errorpages**
  * Added strings for "no network connection" error pages

* **browser-menu**
  * Replaced `BrowserMenuHighlightableItem.Highlight` with `BrowserMenuHighlight.HighPriority` to highlight a menu item with some background color. `Highlight` has been deprecated.
  * Added `BrowserMenuHighlight.LowPriority` to highlight a menu item with a dot over the icon.

* **storage-sync**
  * Added `RemoteTabsStorage` for synced tabs.

* **service-firefox-accounts**
  * Removed `StorageSync` interface as it is superseded by the sync manager.

* **service-glean**
  * Glean was updated to v21.3.0 ([Full changelog](https://github.com/mozilla/glean/compare/v21.2.0...21.3.0))
    * Timers are reset when disabled. That avoids recording timespans across disabled/enabled toggling.
    * Add a new flag to pings: send_if_empty.
    * Upgrade glean_parser to v1.12.0.
    * Implement the deletion request ping in Glean.

# 23.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v22.0.0...v23.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/83?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v23.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v23.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v23.0.0/buildSrc/src/main/java/Config.kt)

* **feature-downloads**
  * ⚠️ **This is a breaking change**:
  * Renamed to `OnDownloadCompleted` to `OnDownloadStopped` for increased clarity on when it's triggered

* **browser-state**
  * ⚠️ **This is a breaking change**: `DownloadState` doesn't include the property `filePath` in its constructor anymore, now it is a computed property. As the previous behavior caused some situations where `fileName` was initially null and after assigned a value to produce `filePath` values like "/storage/emulated/0/Download/null" [for more info](https://sentry.prod.mozaws.net/operations/reference-browser/issues/6609701/).

* **feature-prompts** and **feature-downloads**
  * Fix [issue #6439](https://github.com/mozilla-mobile/fenix/issues/6439) "Crash when downloading Image"

* **service-firefox-accounts**
  * Account profile cache is now used, removing a network call from most instances of account manager instantiation.
  * Fixed a bug where account would disappear after restarting an app which hit authentication problems.
  * Deprecated the `StorageSync` class. Please use the `SyncManager` class instead.

* **service-glean**
  * Glean was updated to v21.2.0
    * Two new metrics were added to investigate sending of metrics and baseline pings.
      See [bug 1597980](https://bugzilla.mozilla.org/show_bug.cgi?id=1597980) for more information.
    * Glean's two lifecycle observers were refactored to avoid the use of reflection.
    * Timespans will now not record an error if stopping after setting upload enabled to false.
    * The `GleanTimerId` can now be accessed in Java and is no longer a `typealias`.
    * Fixed a bug where the metrics ping was getting scheduled twice on startup.
    * When constructing a ping, events are now sorted by their timestamp. In practice,
      it rarely happens that event timestamps are unsorted to begin with, but this
      guards against a potential race condition and incorrect usage of the lower-level
      API.
    * Metrics that can record errors now have a new testing method,
      `testGetNumRecordedErrors`.
    * The experiments API is no longer ignored before the Glean SDK initialized. Calls are
      recorded and played back once the Glean SDK is initialized.
    * String list items were being truncated to 20, rather than 50, bytes when using
      `.set()` (rather than `.add()`). This has been corrected, but it may result
      in changes in the sent data if using string list items longer than 20 bytes.

* **support-base**
  * Deprecated `BackHandler` interface. Use the `UserInteractionHandler.onBackPressed` instead.
  * Added generic `UserInteractionHandler` interface for fragments, features and other components that want to handle user interactions such as ‘back’ or 'home' button presses.

# 22.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v22.0.0...master)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/82?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v22.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v22.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v22.0.0/buildSrc/src/main/java/Config.kt)

* **feature-addons**
  *  ⚠️ **This is a breaking change**:
  * Renamed to `AddOnsCollectionsProvider` to `AddOnCollectionProvider` and added caching support:
  ```kotlin
  val addOnsProvider by lazy {
    // Keeps addon collection response cached and valid for one day
    AddOnCollectionProvider(applicationContext, client, maxCacheAgeInMinutes = 24 * 60)
  }

  // May return a cached result, if available
  val addOns = addOnsProvider.getAvailableAddOns()

  // Will never return a cached result
  val addOns = addOnsProvider.getAvailableAddOns(allowCache = false)
  ```

* **lib-nearby**
  * 🆕 New component for communicating directly between two devices
  using Google Nearby API.

* **sample-nearby-chat**
  * 🆕 New sample program demonstrating use of `lib-nearby`.

* **feature-customtabs**
  * ⚠️ `CustomTabWindowFeature` now takes `Activity` instead of `Context`.

* **concept-sync**, **service-firefox-accounts**
  * `OAuthAccount@authorizeOAuthCode` method is now `authorizeOAuthCodeAsync`.

* **service-firefox-accounts**
  * For supported Android API levels (23+), `FxaAccountManager` can now be configured to encrypt persisted FxA state, via `secureStateAtRest` flag on passed-in `DeviceConfig`. Defaults to `false`. For lower API levels, setting `secureStateAtRest` will continue storing FxA state in plaintext. If the device is later upgraded to 23+, FxA state will be automatically migrated to an encrypted storage.
  * FxA state is stored in application's data directory, in plaintext or encrypted-at-rest if configured via the `secureStateAtRest` flag. This state contains everything that's necessary to download and decrypt data stored in Firefox Sync.
  * An instance of a `CrashReporter` may now be passed to the `FxaAccountManager`'s constructor. If configured, it will be used to report any detected abnormalities.
  *  ⚠️ **This is a breaking change**:
  * Several `FxaAccountManager` methods have been made internal, and are no longer part of the public API of this module: `createSyncManager`, `getAccountStorage`.

# 21.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v20.0.0...v21.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/81?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v21.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v21.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v21.0.0/buildSrc/src/main/java/Config.kt)

* **feature-downloads**
  * Added `tryAgain` which can be called on the feature in order to restart a failed download.

* **lib-dataprotect**
  * Added new `SecureAbove22Preferences` helper class, which is an encryption-aware wrapper for `SharedPreferences`. Only actually encrypts stored values when running on API23+.

* **service-firefox-accounts**
  * Support for keeping `SyncEngine.Passwords` engine unlocked during sync. If you're syncing this engine, you must use `SecureAbove22Preferences` to store encryption key (stored under "passwords" key), and pass an instance of these secure prefs to `GlobalSyncableStoreProvider.configureKeyStorage`.

* **concept-sync**
  * Added new `LockableStore` to facilitate syncing of "lockable" stores (such as `SyncableLoginsStore`).

* **feature-sitepermissions**
  * Added a new get operator to `SitePermissions` to facilitate the retrieval of permissions.
  ```kotlin
    val sitePermissions = SitePermissions(
            "dev.mozilla.org",
            notification = ALLOWED,
            savedAt = 0)

    sitePermissions[Permission.LOCATION] //  ALLOWED will be returned
  ```
* **engine-gecko-nightly**
  * Adds setDynamicToolbarMaxHeight ApI.

* **feature-push**
  * Added `unsubscribeAll` support from the Rust native layer.

# 20.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v19.0.0...v20.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/80?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v20.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v20.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v20.0.0/buildSrc/src/main/java/Config.kt)

* **browser-session**, **feature-customtabs**, **feature-session**, **feature-tabs**
  *  ⚠️ **This is a breaking change**: The `WindowFeature` and `CustomTabWindowFeature` components have been migrated to `browser-state` from `browser-session`. Therefore creating these features now requires a `BrowserStore` instance (instead of a `SessionManager` instance). The `windowRequest` properties have been removed `Session` so window requests can now only be observed on a `BrowserStore` from the `browser-state` component. In addition, `WindowFeature` was moved from `feature-session` to `feature-tabs` because it now makes use of our `TabsUseCases` and this would otherwise cause a dependency cycle.

* **feature-downloads**
  * Added ability to pause, resume, cancel, and try again on a download through the `DownloadNotification`.
  * Added support for multiple, continuous downloads.
  * Added size of the file to the `DownloadNotification`.
  * Added open file functionality to the `DownloadNotification`.
    * Note: you must add a `FileProvider` to your manifest as well as `file_paths.xml`. See SampleBrowser for an example.
    * To open .apk files, you must still add the permission `android.permission.INSTALL_PACKAGES` to your manifest.
  * Improved visuals of `SimpleDownloadDialogFragment` to better match `SitePermissionsDialogFragment`.
    * `SimpleDownloadDialogFragment` can similarly be themed by using `PromptsStyling` properties.
  * Recreated download notification channel with lower importance for Android O+ so that the notification is not audibly intrusive.

* **feature-webnotifications**
  * Adds feature implementation for configuring and displaying web notifications to the user
  ```kotlin
  WebNotificationFeature(
      applicationContext, engine, icons, R.mipmap.ic_launcher, BrowserActivity::class.java
  )
  ```

* **service-glean**
   * Bumped the Glean SDK version to 19.1.0. This fixes a startup crash on Android SDK 22 devices due to missing `stderr`.

* **concept-engine**
  * Adds support for WebPush abstraction to the Engine.
  * Adds support for WebShare abstraction as a PromptRequest.

* **engine-gecko-nightly**
  * Adds support for WebPush in GeckoEngine.

* **support-webextensions**
  * Adds support for sending messages to background pages and scripts in WebExtensions.

* **service-firefox-accounts**
  * Adds `authorizeOAuthCode` method for generating scoped OAuth codes.

* **feature-push**
  * ⚠️ The `AutoPushFeature` now throws when reaching exceptions in the native layer that are unrecoverable.

* **feature-prompts**
  * Adds support for Web Share API using `ShareDelegate`.

* **experiments**
  * Fixes a crash when the app version or the experiment's version specifiers are not in the expected format.

* **engine**, **engine-gecko-***, **support-webextensions**
  * Added support `browser.tabs.remove()` in web extensions.
  ```kotlin
  val engine = GeckoEngine(applicationContext, engineSettings)
  engine.registerWebExtensionTabDelegate(object : WebExtensionTabDelegate {
    override fun onCloseTab(webExtension: WebExtension?, engineSession: EngineSession) {
      store.state.tabs.find { it.engineState.engineSession === engineSession }?.let {
        store.dispatch(RemoveTabAction(tab.id))
      }
    }
  })
  ```

# 19.0.1

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v19.0.0...v19.0.1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v19.0.1/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v19.0.1/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v19.0.1/buildSrc/src/main/java/Config.kt)

* **service-glean**
   * Bumped the Glean SDK version to 19.1.0. This fixes a startup crash on Android SDK 22 devices due to missing `stderr`.

# 19.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v18.0.0...v19.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/79?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v19.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v19.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v19.0.0/buildSrc/src/main/java/Config.kt)

* **browser-toolbar**
  * ⚠️ **This is a breaking change**: Refactored the internals to use `ConstraintLayout`. As part of this change the public API was simplified and unused methods/properties have been removed.

* **feature-accounts**
  * Add new `FxaPushSupportFeature` for some underlying support when connecting push and fxa accounts together.

* **browser-state**
  * Added `externalAppType` to `CustomTabConfig` to indicate how the session is being used.

* **service-glean**
   * The Rust implementation of the Glean SDK is now being used.
   * ⚠️ **This is a breaking change**: the `GleanDebugActivity` is no longer exposed from service-glean. Users need to use the one in `mozilla.telemetry.glean.debug.GleanDebugActivity` from the `adb` command line.

* **lib-push-firebase**
   * Fixes a potential bug where we receive a message for another push service that we cannot process.

* **feature-privatemode**
  * Added new feature for private browsing mode.
  * Added `SecureWindowFeature` to prevent screenshots in private browsing mode.

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * **Merge day!**
    * `browser-engine-gecko`: GeckoView 70.0
    * `browser-engine-gecko-beta`: GeckoView 71.0
    * `browser-engine-gecko-nightly`: GeckoView 72.0

* **feature-push**
  * The `AutoPushFeature` now checks (once every 24 hours) to verify and renew push subscriptions if expired after a cold boot.

# 18.0.1

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v18.0.0...v18.0.1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v18.0.1/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v18.0.1/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v18.0.1/buildSrc/src/main/java/Config.kt)

* **feature-prompts**
  * Fixed a crash when showing the file picker.

# 18.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v17.0.0...v18.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/78?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v18.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v18.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v18.0.0/buildSrc/src/main/java/Config.kt)

* **browser-menu**
  * Adds the ability to create a BrowserMenuCategory, a menu item that defines a category for other menu items

* **concept-engine**
  * Adds the setting `forceUserScalableContent`.

* **engine-gecko-nightly**
  * Implements the setting `forceUserScalableContent`.

* **feature-prompts**
  * Deprecated `PromptFeature` constructor that has parameters for both `Activity` and `Fragment`. Use the constructors that just take either one instead.
  * Changed `sessionId` parameter name to `customTabId`.

# 17.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v16.0.0...v17.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/77?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v17.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v17.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v17.0.0/buildSrc/src/main/java/Config.kt)

* **feature-contextmenu**
  * The "Save Image" context menu item will no longer prompt before downloading the image.

* **concept-engine**
  * Added `WebAppManifest.ShareTarget` data class.

* **lib-crash**
  * Now supports sending caught exceptions.  Use the 'submitCaughtException()' to send caught exceptions if the underlying crash reporter service supports it.
  ```kotlin
  val job = crashReporter.submitCaughtException(e)
  ```

* **engine**, **engine-gecko-nightly**, **engine-gecko-beta**, **engine-gecko**
  * ⚠️ **This is a breaking change**: Renamed `WebExtensionTabDelegate` to `WebExtensionDelegate` to handle various web extensions related engine events:
  ```kotlin
  GeckoEngine(applicationContext, engineSettings).also {
    it.registerWebExtensionDelegate(object : WebExtensionDelegate {
        override fun onNewTab(webExtension: WebExtension?, url: String, engineSession: EngineSession) {
          sessionManager.add(Session(url), true, engineSession)
        }
    })
  }
  ```
  * ⚠️ **This is a breaking change**: Redirect source and target flags are now passed to history tracking delegates. As part of this change, `HistoryTrackingDelegate.onVisited()` receives a new `PageVisit` data class as its second argument, specifying the `visitType` and `redirectSource`. For more details, please see [PR #4268](https://github.com/mozilla-mobile/android-components/pull/4268).

* **support-webextensions**
  * Added functionality to make sure web extension related events in the engine are reflected in the browser state / store. Instead of attaching a `WebExtensionDelegate` to the engine, and manually reacting to all events, it's now possible to initialize `WebExtensionSupport`, which provides overridable default behaviour for all web extension related engine events:
  ```kotlin
  // Makes sure web extension related events (e.g. an extension is installed, or opens a new tab) are dispatched to the browser store.
  WebExtensionSupport.initialize(components.engine, components.store)

  // If dispatching to the browser store is not desired, all actions / behaviour can be overridden:
  WebExtensionSupport.initialize(components.engine, components.store, onNewTabOverride = {
        _, engineSession, url -> components.sessionManager.add(Session(url), true, engineSession)
  })
  ```

* **browser-menu**
   * Fixes background ripple of Switch in BrowserMenuImageSwitch

# 16.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v15.0.0...v16.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/76?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v16.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v16.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v16.0.0/buildSrc/src/main/java/Config.kt)

* **feature-awesomebar**
  * ⚠️ **This is a breaking change**: `AwesomeBar.Suggestion` now directly takes a Bitmap for the icon param rather than a Unit.

* **feature-pwa**
  * ⚠️ **This is a breaking change**: Intent sent from the `WebAppShortcutManager` now require the consumption of the `SHORTCUT_CATEGORY` in your manifest

* **feature-customtabs**
  * 'CustomTabIntentProcessor' can create private sessions now.

* **browser-session**, **browser-state**, **feature-prompts**
  *  ⚠️ **This is a breaking change**: The `feature-prompts` component has been migrated to `browser-state` from `browser-session`. Therefore creating a `PromptFeature` requires a `BrowserStore` instance (instead of a `SessionManager` instance). The `promptRequest` property has been removed `Session`. Prompt requests can now only be observed on a `BrowserStore` from the `browser-state` component.

* **tooling-detekt**
  * Published detekt rules for internal use. Check module documentation for detailed ruleset description.

* **feature-intent**
  * Added support for NFC tag intents to `TabIntentProcessor`.

* **firefox-accounts**, **service-fretboard**
  * ⚠️ **This is a breaking change**: Due to migration to WorkManager v2.2.0, some classes like `WorkManagerSyncScheduler` and `WorkManagerSyncDispatcher` now expects a `Context` in their constructors.

* **engine**, **engine-gecko-nightly** and **engine-gecko-beta**
  * Added `WebExtensionsTabsDelegate` to support `browser.tabs.create()` in web extensions.
  ```kotlin
  GeckoEngine(applicationContext, engineSettings).also {
    it.registerWebExtensionTabDelegate(object : WebExtensionTabDelegate {
        override fun onNewTab(webExtension: WebExtension?, url: String, engineSession: EngineSession) {
          sessionManager.add(Session(url), true, engineSession)
        }
    })
  }
  ```

# 15.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v14.0.0...v15.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/75?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v15.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v15.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v15.0.0/buildSrc/src/main/java/Config.kt)

* **browser-session**, **browser-state**, **feature-contextmenu**, **feature-downloads**
  * * ⚠️ **This is a breaking change**: Removed the `download` property from `Session`. Downloads can now only be observed on a `BrowserState` from the `browser-state` component. Therefore `ContextMenuUseCases` and `DownloadsUseCases` now require a `BrowserStore` instance.

* **support-ktx**
  * Adds `Resources.Theme.resolveAttribute(Int)` to quickly get a resource ID from a theme.
  * Adds `Context.getColorFromAttr` to get a color int from an attribute.

* **feature-customtabs**
  * Added `CustomTabWindowFeature` to handle windows inside custom tabs, PWAs, and TWAs.

* **feature-tab-collections**

  * Behavior change: In a collection List<TabEntity> is now ordered descending by creation date (newest tab in a collection on top)
* **feature-session**, **engine-gecko-nightly** and **engine-gecko-beta**
  * Added api to manage the tracking protection exception list, any session added to the list will be ignored and the the current tracking policy will not be applied.

  ```kotlin
    val useCase = TrackingProtectionUseCases(sessionManager,engine)

    useCase.addException(session)

    useCase.removeException(session)

    useCase.removeAllExceptions()

    useCase.containsException(session){ contains ->
        // contains indicates if this session is on the exception list.
    }

    useCase.fetchExceptions { exceptions ->
        // exceptions is a list of all the origins that are in the exception list.
    }
  ```

* **support-sync-telemetry**
  * 🆕 New component containing building blocks for sync telemetry.

* **concept-sync**, **services-firefox-accounts**
  ⚠️ **This is a breaking change**
  * Internal implementation of sync changed. Most visible change is that clients are now allowed to change which sync engines are enabled and disabled.
  * `FxaAccountManager#syncNowAsync` takes an instance of a `reason` instead of `startup` boolean flag.
  * `SyncEnginesStorage` is introduced, allowing applications to read and update enabled/disabled state configured `SyncEngine`s.
  * `SyncEngine` is no longer an `enum class`, but a `sealed class` instead. e.g. `SyncEngine.HISTORY` is now `SyncEngine.History`.
  * `DeviceConstellation#setDeviceNameAsync` now takes a `context` in addition to new `name`.
  * `FxaAuthData` now takes an optional `declinedEngines` set of SyncEngines.

# 14.0.1

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v14.0.0...v14.0.1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v14.0.1/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v14.0.1/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v14.0.1/buildSrc/src/main/java/Config.kt)

* **feature-collections**
  * Fixed [#4514](https://github.com/mozilla-mobile/android-components/issues/4514): Do not restore parent tab ID for collections.

* **service-glean**
  *  PR [#4511](https://github.com/mozilla-mobile/android-components/pull/4511/): Always set 'wasMigrated' to false in the Glean SDK.

# 14.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v13.0.0...v14.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/74?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v14.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v14.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v14.0.0/buildSrc/src/main/java/Config.kt)

* **feature-customtabs**
  * Now the color of the tracking protection icon adapts to color of the toolbar.

* **feature-session**, **engine-gecko-nightly** and **engine-gecko-beta**
  * Added a way to exposes the same amount of trackers as Firefox desktop has in it tracking protection panel via TrackingProtectionUseCases.

  ```kotlin
    val useCase = TrackingProtectionUseCases(sessionManager,engine)
    useCase.fetchTrackingLogs(
        session,
        onSuccess = { trackersLog ->
            // A list of all the tracker logger for this session
        },
        onError = { throwable ->
            //A throwable indication what went wrong
        }
    )
  ```

* **browser-toolbar**
  * Resized icons on the toolbar see [#4490](https://github.com/mozilla-mobile/android-components/issues/4490) for more information.
  * Added a way to customize the color of the tracking protection icon via BrowserToolbar.
  ```kotlin
  val toolbar = BrowserToolbar(context)
  toolbar.trackingProtectionColor = Color.BLUE
  ```

* **All components**
  * Increased `androidx.browser` version to `1.2.0-alpha07`.

* **feature-media**
  * Playback will now be stopped and the media notification will get removed if the app's task is getting removed (app is swiped away in task switcher).

* **feature-pwa**
  * Adds `WebAppHideToolbarFeature.onToolbarVisibilityChange` to be notified when the toolbar is shown or hidden.

* **engine-gecko-nightly**
  * Added the ability to exfiltrate Gecko categorical histograms.

* **support-webextensions**
  * 🆕 New component containing building blocks for features implemented as web extensions.

* **lib-push-amazon**
  * Fixed usage of cache version of registration ID in situations when app data is deleted.

* **tools-detekt**
  * New (internal-only) component with custom detekt rules.

* **service-glean**
  * ⚠ **This is a breaking change**: Glean.initialize() must be called on the main thread.

# 13.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v12.0.0...v13.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/73?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v13.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v13.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v13.0.0/buildSrc/src/main/java/Config.kt)

* **All components**
  * Updated Kotlin version from `1.3.40` to `1.3.50`.
  * Updated Kotlin Coroutine library version from `1.2.2` to `1.3.0`.

* **browser-session**
  * Clear session icon only if URL host changes.

* **feature-pwa**
  * Adds `WebAppUseCases.isInstallable` to check if the current session can be installed as a Progressive Web App.

* **feature-downloads**
  *  ⚠️ **This is a breaking change**: The `feature-downloads` component has been migrated to `browser-state` from `browser-session`. Therefore creating a `DownloadsFeature` requires a `BrowserStore` instance (instead of a `SessionManager` instance) and a `DownloadsUseCases` instance now.

* **feature-contextmenu**
  *  ⚠️ **This is a breaking change**: The `feature-contextmenu` component has been migrated to `browser-state` from `browser-session`. Therefore creating a `ContextMenuFeature` requires a `BrowserStore` instance (instead of a `SessionManager` instance) and a `ContextMenuUseCases` instance now.

* **service-glean**
  * ⚠️ **This is a breaking change**: applications need to use `ConceptFetchHttpUploader` for overriding the ping uploading mechanism instead of directly using `concept-fetch` implementations.

* **feature-tabs**
  * ⚠️ **This is a breaking change**: Methods that have been accepting a parent `Session` parameter now expect the parent id (`String`).

* **browser-menu**
   * Adds the ability to create a BrowserMenuImageSwitch, a BrowserMenuSwitch with icon

* **feature-accounts**
  * Added ability to configure FxaWebChannelFeature with a set of `FxaCapability`. Currently there's just one: `CHOOSE_WHAT_TO_SYNC`. It defaults to `false`, so if you want "choose what to sync" selection during auth flows, please specify it.

# 12.0.1

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v12.0.0...v12.0.1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v12.0.1/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v12.0.1/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v12.0.1/buildSrc/src/main/java/Config.kt)

* **lib-push-amazon**
  * Fixed [#4448](https://github.com/mozilla-mobile/android-components/issues/4458): Clearing app data does not reset the registration ID.

# 12.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v11.0.0...v12.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/72?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v12.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v12.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v12.0.0/buildSrc/src/main/java/Config.kt)

* **browser-engine-gecko-nightly**, **browser-engine-gecko-beta** and **browser-engine-gecko**
  * The `TrackingProtectionPolicy.recommended()` and `TrackingProtectionPolicy.strict()`  policies are now aligned with standard and strict (respectively) policies on FireFox desktop, for more details see the [issue #4349](https://github.com/mozilla-mobile/android-components/issues/4349).

* **browser-engine-gecko-nightly** and **browser-engine-gecko-beta**
  * The `TrackingProtectionPolicy.select` function now allows you to indicate if `strictSocialTrackingProtection` should be activated or not. When it is active blocks trackers from the social-tracking-protection-digest256 list, for more details take a look at the [issue #4320](https://github.com/mozilla-mobile/android-components/issues/4320)
  ```kotlin
  val policy = TrackingProtectionPolicy.select(
    strictSocialTrackingProtection = true
  )
  ```

* **context-menu**
  * Exposed title tag from GV in HitResult. Fixes [#1444]. If title is null or blank the src value is returned for title.

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * **Merge day!**
    * `browser-engine-gecko-release`: GeckoView 69.0
    * `browser-engine-gecko-beta`: GeckoView 70.0
    * `browser-engine-gecko-nightly`: GeckoView 71.0

* **feature-toolbar**
  *  ⚠️ **This is a breaking change**: The `feature-toolbar` component has been migrated to `browser-state` from `browser-session`. Therefore creating a `ToolbarFeature` requires a `BrowserStore` instance instead of a `SessionManager` instance now.

* **lib-crash**
  * Now supports Breadcrumbs.  Use the 'recordCrashBreadcrumb()' to record Breadcrumbs if the underlying crash reporter service supports it.
  ```kotlin
  crashReporter.recordCrashBreadcrumb(
      CrashBreadcrumb("Settings button clicked", data, "UI", Level.INFO, Type.USER)
  )
  ```

* **browser-engine-gecko-nightly** and **browser-engine-gecko-beta**
  * The `TrackingProtectionPolicy.strict()` now blocks trackers from the social-tracking-protection-digest256 list, for more details take a look at the [issue #4213](https://github.com/mozilla-mobile/android-components/issues/4213)

* **browser-session**
  *  ⚠️ **This is a breaking change**: `getSessionId` and `EXTRA_SESSION_ID` has moved to the `feature-intent` component.

* **feature-intent**
  *  ⚠️ **This is a breaking change**: `TabIntentProcessor` has moved to the `processing` sub-package, but is still in the same component.

* **browser-engine-gecko**
  * Like with the nightly and beta flavor previously this component now has a hard dependency on the new [universal GeckoView build](https://bugzilla.mozilla.org/show_bug.cgi?id=1508976) that is no longer architecture specific (ARM, x86, ..). With that apps no longer need to specify the GeckoView dependency themselves and synchronize the used version with Android Components. Additionally apps can now make use of [APK splits](https://developer.android.com/studio/build/configure-apk-splits) or [Android App Bundles (AAB)](https://developer.android.com/guide/app-bundle).

* **browser-engine-servo**
  * ❌ We removed the `browser-engine-servo` component since it was not being maintained, updated and used.

* **concept-sync**, **service-firefox-accounts**
  * ⚠️ **This is a breaking change**:
  * `SyncConfig`'s `syncableStores` has been renamed to `supportedEngines`, expressed via new enum type `SyncEngine`.
  * `begin*` OAuthAccount methods now return an `AuthFlowUrl`, which encapsulates an OAuth state identifier.
  * `AccountObserver:onAuthenticated` method now has `authType` parameter (instead of `newAccount`), which describes in detail what caused an authentication.
  * `GlobalSyncableStoreProvider.configureStore` now takes a pair of `Pair<SyncEngine, SyncableStore>`, instead of allowing arbitrary string names for engines.
  * `GlobalSyncableStoreProvider.getStore` is no longer part of the public API.

* **feature-push**
  * Added more logging into `AutoPushFeature` to aid in debugging in release builds.

* **support-ktx**
  * Added variant of `Flow.ifChanged()` that takes a mapping function in order to filter items where the mapped value has not changed.

* **feature-pwa**
  * Adds the ability to create a basic shortcut with a custom label

* **browser-engine-gecko-nightly**
  * Adds support for exposing Gecko scalars through the Glean SDK. See [bug 1579365](https://bugzilla.mozilla.org/show_bug.cgi?id=1579365) for details.

* **support-utils**
  * `Intent.asForegroundServicePendingIntent(Context)` extension method to create pending intent for the service that will play nicely with background execution limitations introduced in Android O (e.g. foreground service).

* **concept-sync**
  * ⚠️ **This is a breaking change**: `action` param of `AuthType.OtherExternal` is now optional. Missing `action` indicates that we really don't know what external authType we've hit.

# 11.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v10.0.0...v11.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/71?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v11.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v11.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v11.0.0/buildSrc/src/main/java/Config.kt)

* **browser-icons**
  * Ensures icons are not cached on the disk in private sessions.

* **browser-menu**
  * Added `startImage` to Highlight of HighlightableMenuItem, which allows changing the startImage in addition to the endImage when highlighted
  * Highlight properties of HighlightableMenuItem `startImage` and `endImage` are now both optional

* **lib-state**
  * Added `Store` extensions to observe `State` using Kotlin's `Flow` API: `Store.flow()`, `Store.flowScoped()`.

* **support-ktx**
  * Added property delegates to work with `SharedPreferences`.
  * Added `Flow.ifChanged()` operator for filtering a `Flow` based on whether a value has changed from the previous one (e.g. `A, A, B, C, A -> A, B, C, A`).

* **feature-customtabs**
  * Added `CustomTabsServiceStore` to track custom tab data in `AbstractCustomTabsService`.

* **feature-pwa**
  * Added support for hiding the toolbar in a Trusted Web Activity.
  * Added `TrustedWebActivityIntentProcessor` to process TWA intents.
  * Added `CustomTabState.trustedOrigins` extension method to turn the verification state of a custom tab into a list of origins.
  * Added `WebAppHideToolbarFeature.onTrustedScopesChange` to change the trusted scopes after the feature is created.

* **service-telemetry**
  * This component is now deprecated. Please use the [Glean SDK](https://mozilla.github.io/glean/book/index.html) instead. This library will not be removed until all projects using it start using the Glean SDK.

* **browser-session**, **feature-intent**
  * ⚠️ **This is a breaking change**: Moved `Intent` related code from `browser-session` to `feature-intent`.

* **feature-media**
  * The `Intent` launched from the media notification now has its action set to `MediaFeature.ACTION_SWITCH_TAB`. In addition to that the extra `MediaFeature.EXTRA_TAB_ID` contains the id of the tab the media notification is displayed for.

# 10.0.1

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v10.0.0...v10.0.1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v10.0.1/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v10.0.1/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v10.0.1/buildSrc/src/main/java/Config.kt)

* **browser-menu**
  * Added `startImage` to Highlight of HighlightableMenuItem, which allows changing the startImage in addition to the endImage when highlighted
  * Highlight properties of HighlightableMenuItem `startImage` and `endImage` are now both optional

* Imported latest state of translations.

# 10.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v9.0.0...v10.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/69?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v10.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v10.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v10.0.0/buildSrc/src/main/java/Config.kt)

* **feature-toolbar**
  * Toolbar Menu is now closed on exiting the app.

* **support-test-appservices**
  * 🆕 New component for synchronizing Application Services' unit testing dependencies used in Android Components.

* **service-location**
  * Added `RegionSearchLocalizationProvider` - A `SearchLocalizationProvider` implementation that uses a `MozillaLocationService` instance to do a region lookup via GeoIP.
  * ⚠️ **This is a breaking change**: An implementation of `SearchLocalizationProvider` now returns a `SearchLocalization` data class instead of multiple properties.

* **service-glean**
  * ⚠️ **This is a breaking change**: `Glean.handleBackgroundEvent` is now an internal API.
  * Added a `QuantityMetricType` (for internal use by Gecko metrics only).

* **browser-engine-gecko(-beta/nightly)**, **concept-engine**
  * Added simplified `Media.state` derived from `Media.playbackState` events.

* **lib-push-adm**, **lib-push-firebase**, **concept-push**
  * Added `isServiceAvailable` to signify if the push service is supported on the device.

* **concept-engine**
  * Added `WebNotification` data class for the web notifications API.

* **browser-engine-system**
  * Fixed issue [4191](https://github.com/mozilla-mobile/android-components/issues/4191) where the `recommended()` tracking category was not getting applied for `SystemEngine`.

* **concept-engine**, **browser-engine-gecko-nightly** and **browser-engine-gecko-beta**:
  * ⚠️ **This is a breaking change**: `TrackingProtectionPolicy` does not have a `safeBrowsingCategories` anymore, Safe Browsing is now a separate setting on the Engine level. To change the default value of `SafeBrowsingPolicy.RECOMMENDED` you have set it through `engine.settings.safeBrowsingPolicy`.
  * This decouples the tracking protection API and safe browsing from each other so you can change the tracking protection policy without affecting your safe browsing policy as described in this issue [#4190](https://github.com/mozilla-mobile/android-components/issues/4190).
  * ⚠️ **Alert for SystemEngine consumers**: The Safe Browsing API is not yet supported on this engine, this will be covered on [#4206](https://github.com/mozilla-mobile/android-components/issues/4206). If you use this API you will get a `UnsupportedSettingException`, however you can use a manifest tag to activate it.

  ```xml
    <manifest>
    <application>
        <meta-data android:name="android.webkit.WebView.EnableSafeBrowsing"
                   android:value="true" />
        ...
     </application>
    </manifest>
  ```

# 9.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v8.0.0...v9.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/68?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v9.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v9.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v9.0.0/buildSrc/src/main/java/Config.kt)

* **browser-menu**
  * Updated the styling of the menu to not have padding on top or bottom. Also modified size of `BrowserMenuItemToolbar` to match `BrowserToolbar`'s height

* **feature-media**
  * Do not display title/url/icon of website in media notification if website is opened in private mode.

* **concept-engine** and **browser-session**
  * ⚠️ **This is a breaking change**: `TrackingProtectionPolicy` removes the `categories` property to expose two new ones `trackingCategories: Array<AntiTrackingCategory>` and `safeBrowsingCategories: Array<SafeBrowsingCategory>` to separate the tracking protection categories from the safe browsing ones.
  * ⚠️ **This is a breaking change**: `TrackingProtectionPolicy.all()` has been replaced by `TrackingProtectionPolicy.strict()` to have similar naming conventions with GeckoView api.
  * ⚠️ **This is a breaking change**: `Tracker#categories` has been replaced by `Tracker#trackingCategories` and `Tracker#cookiePolicies` to better report blocked content see [#4098](https://github.com/mozilla-mobile/android-components/issues/4098).
  * Added: `Session#trackersLoaded` A list of `Tracker`s that could be blocked but has been loaded in this session.
  * Added: `Session#Observer#onTrackerLoaded` Notifies that a tracker that could be blocked has been loaded.

* **browser-toolbar**
  * HTTP sites are now marked as insecure with a broken padlock icon, rather than a globe icon. Apps can revert to the globe icon by using a custom `BrowserToolbar.siteSecurityIcon`.

* **service-firefox-accounts**, **concept-sync**
  * `FxaAccountManager`, if configured with `DeviceCapability.SEND_TAB`, will now automatically refresh device constellation state and poll for device events during initialization and login.
  * `FxaAccountManager.syncNowAsync` can now receive a `debounce` parameter, allowing consumers to specify debounce behaviour of their sync requests.
  * ⚠️ **This is a breaking change:**
  * Removed public methods from `DeviceConstellation` and its implementation in `FxaDeviceConstellation`: `fetchAllDevicesAsync`, `startPeriodicRefresh`, `stopPeriodicRefresh`.
  * `DeviceConstellation#refreshDeviceStateAsync` was renamed to `refreshDevicesAsync`: no longer polls for device events, only updates device states (e.g. new devices, name changes)
  * `pollForEventsAsync` no longer returns the events. Use the observer API instead:
  ```kotlin
  val deviceConstellation = autheneticatedAccount()?.deviceConstellation() ?: return
  deviceConstellation.registerDeviceObserver(
    object: DeviceEventsObserver {
      override fun onEvents(events: List<DeviceEvent>) {
          // Process device events here.
      }
    }, lifecycleOwner, false)
  // Poll for events.
  deviceConstellation.pollForEventsAsync().await()
  ```

* **browser-session**
  * Removed deprecated `CustomTabConfig` helpers. Use the equivalent methods in **feature-customtabs** instead.

* **support-ktx**
  * Removed deprecated methods that have equivalents in Android KTX.

* **concept-sync**, **service-firefox-account**
  * ⚠️ **This is a breaking change**
  * In `OAuthAccount` (and by extension, `FirefoxAccount`) `beginOAuthFlowAsync` no longer need to specify `wantsKeys` parameter; it's automatically inferred from the requested `scopes`.
  * Three new device types now available: `tablet`, `tv`, `vr`.

# 8.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v7.0.0...v8.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/67?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v8.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v8.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v8.0.0/buildSrc/src/main/java/Config.kt)

* **service-glean**
  * Timing distributions now use a functional bucketing algorithm that does not require fixed limits to be defined up front.

* **support-android-test**
  * Added `WebserverRule` - a junit rule that will run a webserver during tests serving content from assets in the test package ([#3893](https://github.com/mozilla-mobile/android-components/issues/3893)).

* **browser-engine-gecko-nightly**
  * The component now exposes an implementation of the Gecko runtime telemetry delegate, `glean.GeckoAdapter`, which can be used to collect Gecko metrics with the Glean SDK.

* **browser-engine-gecko-beta**
  * The component now handles situations where the Android system kills the content process (without killing the main app process) in order to reclaim resources. In those situations the component will automatically recover and restore the last known state of those sessions.

* **browser-toolbar**
  * Changed `BrowserToolbar.siteSecurityColor` to use no icon color filter when the color is set to `Color.TRANSPARENT`.
  * Added `BrowserToolbar.siteSecurityIcon` to use custom security icons with multiple colors in the toolbar.

* **feature-sendtab**
  * Added a `SendTabFeature` that observes account device events with optional support for push notifications.

  ```kotlin
  SendTabFeature(
    context,
    accountManager,
    pushFeature, // optional
    pushService // optional; if you want the service to also be started/stopped based on account changes.
    onTabsReceiver = { from, tabs -> /* Do cool things here! */ }
  )
  ```

* **feature-media**
  * `MediaFeature` is no longer showing a notification for playing media with a very short duration.
  * Lowered priority of media notification channel to avoid the media notification making any sounds itself.

# 7.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v6.0.2...v7.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/66?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v7.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v7.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v7.0.0/buildSrc/src/main/java/Config.kt)

* **browser-menu**
  * ⚠️ **This is a breaking change**: `BrowserMenuHighlightableItem` now has a ripple effect and includes an example of how to pass in a drawable properly to also include a ripple when highlighted

* **feature-accounts**
    * ⚠️ **This is a breaking change**:
    * The `FirefoxAccountsAuthFeature` no longer needs an `TabsUseCases`, instead is taking a lambda to
      allow applications to decide which action should be taken. This fixes [#2438](https://github.com/mozilla-mobile/android-components/issues/2438) and [#3272](https://github.com/mozilla-mobile/android-components/issues/3272).

    ```kotlin
     val feature = FirefoxAccountsAuthFeature(
         accountManager,
         redirectUrl
     ) { context, authUrl ->
        // passed-in context allows easily opening new activities for handling urls.
        tabsUseCases.addTab(authUrl)
     }

     // ... elsewhere, in the UI code, handling click on button "Sign In":
     components.feature.beginAuthentication(activityContext)
    ```

* **browser-engine-gecko-nightly**
  * Now supports window requests. A new tab will be opened for `target="_blank"` links and `window.open` calls.

* **browser-icons**
  * Handles low-memory scenarios by reducing memory footprint.

* **feature-app-links**
  * Fixed [#3944](https://github.com/mozilla-mobile/android-components/issues/3944) causing third-party apps being opened when links with a `javascript` scheme are clicked.

* **feature-session**
  * ⚠️ **This is a breaking change**:
  * The `WindowFeature` no longer needs an engine. It can now be created using just:
  ```kotlin
     val windowFeature = WindowFeature(components.sessionManager)
  ```

* **feature-pwa**
  * Added full support for pinning websites to the home screen.
  * Added full support for Progressive Web Apps, which can be pinned and open in their own window.

* **service-glean**
  * Fixed a bug in`TimeSpanMetricType` that prevented multiple consecutive `start()`/`stop()` calls. This resulted in the `glean.baseline.duration` being missing from most [`baseline`](https://mozilla.github.io/glean/book/user/pings/baseline.html) pings.

* **service-firefox-accounts**
  * ⚠️ **This is a breaking change**: `AccountObserver.onAuthenticated` now helps observers distinguish when an account is a new authenticated account one with a second `newAccount` boolean parameter.

* **concept-sync**, **service-firefox-accounts**:
  * ⚠️ **This is a breaking change**: Added `OAuthAccount@disconnectAsync`, which replaced `DeviceConstellation@destroyCurrentDeviceAsync`.

* **lib-crash**
  * ⚠️ **Known issue**: Sending a crash using the `MozillaSocorroService` with GeckoView 69.0 or 68.0, will lead to a `NoSuchMethodError` when using this particular version of android components. See [#4052](https://github.com/mozilla-mobile/android-components/issues/4052).

# 6.0.2

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v6.0.1...v6.0.2)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v6.0.2/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v6.0.2/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v6.0.2/buildSrc/src/main/java/Config.kt)

* **service-glean**
  * Fixed a bug in`TimeSpanMetricType` that prevented multiple consecutive `start()`/`stop()` calls. This resulted in the `glean.baseline.duration` being missing from most [`baseline`](https://mozilla.github.io/glean/book/user/pings/baseline.html) pings.

# 6.0.1

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v6.0.0...v6.0.1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v6.0.1/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v6.0.1/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v6.0.1/buildSrc/src/main/java/Config.kt)

* **feature-app-links**
  * Fixed [#3944](https://github.com/mozilla-mobile/android-components/issues/3944) causing third-party apps being opened when links with a `javascript` scheme are clicked.

* Imported latest state of translations.

# 6.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v5.0.0...v6.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/65?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v6.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v6.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v6.0.0/buildSrc/src/main/java/Config.kt)

* **support-utils**
  * Fixed [#3871](https://github.com/mozilla-mobile/android-components/issues/3871) autocomplete incorrectly fills urls that contains a port number.

* **feature-readerview**
  * Fixed [#3864](https://github.com/mozilla-mobile/android-components/issues/3864) now minus and plus buttons have the same size on reader view.

* **browser-engine-gecko-nightly**
  * The component now handles situations where the Android system kills the content process (without killing the main app process) in order to reclaim resources. In those situations the component will automatically recover and restore the last known state of those sessions.
  * Now supports window requests. A new tab will be opened for `target="_blank"` links and `window.open` calls.

* **service-location**
  * 🆕 A new component for accessing Mozilla's and other location services.

* **feature-prompts**
  * Improved month picker UI, now we have the same widget as Fennec.

* **support-ktx**
  * Deprecated `ViewGroup.forEach` in favour of Android Core KTX.
  * Deprecated `Map.toBundle()` in favour of Android Core KTX `bundleOf`.

* **lib-state**
  * Migrated `Store.broadcastChannel()` to `Store.channel()`returning a `ReceiveChannel` that can be read by only one receiver. Broadcast channels have a more complicated lifetime that is not needed in most use cases. For multiple receivers multiple channels can be created from the `Store` or Kotlin's `ReceiveChannel.broadcast()` extension method can be used.

* **support-android-test**
  * Added `LeakDetectionRule` to install LeakCanary when running instrumented tests. If a leak is found the test will fail and the test report will contain the leak trace.

* **lib-push-amazon**
  * 🆕 Added a new component for Amazon Device Messaging push support.

* **browser-icons**
  * Changed the maximum size for decoded icons. Icons are now scaled to the target size to save memory.

* **service-firefox-account**
 * Added `isSyncActive(): Boolean` method to `FxaAccountManager`

* **feature-customtabs**
  * `CustomTabsToolbarFeature` now optionally takes `Window` as a parameter. It will update the status bar color to match the toolbar color.
  * Custom tabs can now style the navigation bar using `CustomTabsConfig.navigationBarColor`.

* **feature-sendtab**
  * 🆕 New component for send tab use cases.

  ```kotlin
    val sendTabUseCases = SendTabUseCases(accountManager)

    // Send to a particular device
    sendTabUseCases.sendToDeviceAsync("1234", TabData("Mozilla", "https://mozilla.org"))

    // Send to all devices
    sendTabUseCases.sendToAllAsync(TabData("Mozilla", "https://mozilla.org"))

    // Send multiple tabs to devices works too..
    sendTabUseCases.sendToDeviceAsync("1234", listof(tab1, tab2))
    sendTabUseCases.sendToAllAsync(listof(tab1, tab2))
  ```

* **support-ktx**
  * Added `Collection.crossProduct` to retrieve the cartesian product of two `Collections`.

* **service-glean**
  * ⚠️ **This is a breaking change**: `Glean.enableTestingMode` is now `internal`. Tests can use the `GleanTestRule` to enable testing mode. [Updated docs available here](https://mozilla.github.io/glean/book/user/testing-metrics.html).

* **feature-push**
  * Added default arguments when registering for subscriptions/messages.

# 5.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v4.0.0...v5.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/64?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v5.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v5.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v5.0.0/buildSrc/src/main/java/Config.kt)

* **All components**
  * Increased `compileSdkVersion` to 29 (Android Q)

* **feature-tab**
  * ⚠️ **This is a breaking change**: Now `TabsUseCases.SelectTabUseCase` is an interface, if you want to rely on its previous behavior you could keep using `TabsUseCases.selectTab` or use `TabsUseCases.DefaultSelectTabUseCase`.

* **feature-awesomebar**
  * `SessionSuggestionProvider` now have a new parameter `excludeSelectedSession`, to ignore the selected session on the suggestions.

* **concept-engine** and **browser-session**
  * ⚠️ **This is a breaking change**: Function signature changed from `Session.Observer.onTrackerBlocked(session: Session, blocked: String, all: List<String>) = Unit` to `Session.Observer.onTrackerBlocked(session: Session, tracker: Tracker, all: List<Tracker>) = Unit`
  * ⚠️ **This is a breaking change**: Function signature changed from `EngineSession.Observer.onTrackerBlocked(url: String) = Unit` to `EngineSession.Observer.onTrackerBlocked(tracker: Tracker) = Unit`
  * Added: To provide more details about a blocked content, we introduced a new class called `Tracker` this contains information like the `url` and `categories` of the `Tracker`. Among the categories we have `Ad`, `Analytic`, `Social`,`Cryptomining`, `Fingerprinting` and `Content`.

* **browser-icons**
  * Added `BrowserIcons.loadIntoView` to automatically load an icon into an `ImageView`.

* **browser-session**
  * Added `IntentProcessor` interface to represent a class that processes intents to create sessions.
  * Deprecated `CustomTabConfig.isCustomTabIntent` and `CustomTabConfig.createFromIntent`. Use `isCustomTabIntent` and `createFromCustomTabIntent` in feature-customtabs instead.

* **feature-customtabs**
  * Added `CustomTabIntentProcessor` to create custom tab sessions from intents.
  * Added `isCustomTabIntent` to check if an intent is for creating custom tabs.
  * Added `createCustomTabConfigFromIntent` to create a `CustomTabConfig` from a custom tab intent.

* **feature-downloads**
  * `FetchDownloadManager` now determines the filename during the download, resulting in more accurate filenames.

* **feature-intent**
  * Deprecated `IntentProcessor` class and moved some of its code to the new `TabIntentProcessor`.

* **feature-push**
  * Updated the default autopush service endpoint to `updates.push.services.mozilla.com`.

* **service-glean**
  * Hyphens `-` are now allowed in labels for metrics.  See [1566764](https://bugzilla.mozilla.org/show_bug.cgi?id=1566764).
  * ⚠️ **This is a breaking change**: Timespan values are returned in their configured time unit in the testing API.

* **lib-state**
  * Added ability to pause/resume observing a `Store` via `pause()` and `resume()` methods on the subscription
  * When using `observeManually` the returned `Subscription` is in paused state by default.
  * When binding a subscription to a `LifecycleOwner` then this subscription will automatically paused and resumed based on whether the lifecycle is in STARTED state.
  * When binding a subscription to a `View` then this subscription will be paused until the `View` gets attached.
  * Added `Store.broadcastChannel()` to observe state from a coroutine sequentially ordered.
  * Added helpers to process states coming from a `Store` sequentially via `Fragment.consumeFrom(Store)` and `View.consumeFrom(Store)`.

* **support-ktx**
  * ⚠️ **This is a breaking behavior change**: `JSONArray.mapNotNull` is now an inline function, changing the behavior of the `return` keyword within its lambda.
  * Added `View.toScope()` to create a `CoroutineScope` that is active as long as the `View` is attached. Once the `View` gets detached the `CoroutineScope` gets cancelled automatically.  By default coroutines dispatched on the created [CoroutineScope] run on the main dispatcher

* **concept-push**, **lib-push-firebase**, **feature-push**
  * Added `deleteToken` to the PushService interface.
  * Added the implementation for it to Firebase Push implementation.
  * Added `forceRegistrationRenewal` to the AutopushFeature for situations where our current registration token may be invalid for us to use.

* **service-firefox-accounts**
  * Added `AccountMigration`, which may be used to query trusted FxA Auth providers and automatically sign-in into available accounts.

# 4.0.1

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v4.0.0...v4.0.1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v4.0.1/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v4.0.1/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v4.0.1/buildSrc/src/main/java/Config.kt)

* **service-glean**
  * Hyphens `-` are now allowed in labels for metrics.  See [1566764](https://bugzilla.mozilla.org/show_bug.cgi?id=1566764).

* Imported latest state of translations.

* **support-rusthttp**
  * ⚠️ **This is a breaking change**: The application-services (FxA, sync, push) code now will send HTTP requests through a kotlin-provided HTTP stack in all configurations, however it requires configuration at startup. This may be done via the neq `support-rusthttp` component as follows:

  ```kotlin
  import mozilla.components.support.rusthttp.RustHttpConfig
  // Note: other implementions of `Client` from concept-fetch are fine as well.
  import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
  // some point before calling rust code that makes HTTP requests.
  RustHttpConfig.setClient(lazy { HttpURLConnectionClient() })
  ```

  * Note that code which uses a custom megazord **must** call this after initializing the megazord.


# 4.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v3.0.0...v4.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/63?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v4.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v4.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v4.0.0/buildSrc/src/main/java/Config.kt)

* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * **Merge day!**
    * `browser-engine-gecko-release`: GeckoView 68.0
    * `browser-engine-gecko-beta`: GeckoView 69.0
    * `browser-engine-gecko-nightly`: GeckoView 70.0

* **browser-engine-gecko-beta**
  * Like with the nightly flavor previously (0.55.0) this component now has a hard dependency on the new [universal GeckoView build](https://bugzilla.mozilla.org/show_bug.cgi?id=1508976) that is no longer architecture specific (ARM, x86, ..). With that apps no longer need to specify the GeckoView dependency themselves and synchronize the used version with Android Components. Additionally apps can now make use of [APK splits](https://developer.android.com/studio/build/configure-apk-splits) or [Android App Bundles (AAB)](https://developer.android.com/guide/app-bundle).

* **feature-media**
  * Added `MediaNotificationFeature` - a feature implementation to show an ongoing notification (keeping the app process alive) while web content is playing media.

* **feature-downloads**
  * Added custom notification icon for `FetchDownloadManager`.

* **feature-app-links**
  * Added whitelist for schemes of URLs to open with an external app. This defaults to `mailto`, `market`, `sms` and `tel`.

* **feature-accounts**
  * ⚠️ **This is a breaking change**: Public API for interacting with `FxaAccountManager` and sync changes
  * `FxaAccountManager` now has a new, simplified public API.
  * `BackgroundSyncManager` is longer exists; sync functionality exposed directly via `FxaAccountManager`.
  * See component's [README](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/README.md) for detailed description of the new API.
  * As part of these changes, token caching issue has been fixed. See [#3579](https://github.com/mozilla-mobile/android-components/pull/3579) for details.

* **concept-engine**, **browser-engine-gecko(-beta/nightly)**.
  * Added `TrackingProtectionPolicy.CookiePolicy` to indicate how cookies should behave for a given `TrackingProtectionPolicy`.
  * Now `TrackingProtectionPolicy.select` allows you to specify a `TrackingProtectionPolicy.CookiePolicy`, if not specified, `TrackingProtectionPolicy.CookiePolicy.ACCEPT_NON_TRACKERS` will be used.
  * Behavior change: Now `TrackingProtectionPolicy.none()` will get assigned a `TrackingProtectionPolicy.CookiePolicy.ACCEPT_ALL`, and both `TrackingProtectionPolicy.all()` and `TrackingProtectionPolicy.recommended()` will have a `TrackingProtectionPolicy.CookiePolicy.ACCEPT_NON_TRACKERS`.

* **concept-engine**, **browser-engine-system**
  * Added `useWideViewPort` in `Settings` to support the viewport HTML meta tag or if a wide viewport should be used. (Only affects `SystemEngineSession`)

* **browser-session**
  * Added `SessionManager.add(List<Session>)` to add a list of `Session`s to the `SessionManager`.

* **feature-tab-collections**
  * ⚠️ **These are breaking changes below**:
  * `Tab.restore()` now returns a `Session` instead of a `SessionManager.Snapshot`
  * `TabCollection.restore()` and `TabCollection.restoreSubset()` now return a `List<Session>` instead of a `SessionManager.Snapshot`

* **support-ktx**
  * Added `onNextGlobalLayout` to add a `ViewTreeObserver.OnGlobalLayoutListener` that is only called once.

# 3.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v2.0.0...v3.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/62?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v3.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v3.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v3.0.0/buildSrc/src/main/java/Config.kt)

* **feature-prompts**
  * Improved file picker prompt by displaying the option to use the camera to capture images,
    microphone to record audio, or video camera to capture a video.
  * The color picker has been redesigned based on Firefox for Android (Fennec).

* **feature-pwa**
  * Added preliminary support for pinning websites to the home screen.

* **browser-search**
  * Loading search engines should no longer deadlock on devices with 1-2 CPUs

* **concept-engine**, **browser-engine-gecko(-beta/nightly)**, **browser-engine-system**
  * Added `EngineView.release()` to manually release an `EngineSession` that is currently being rendered by the `EngineView`. Usually an app does not need to call `release()` manually since `EngineView` takes care of releasing the `EngineSession` on specific lifecycle events. However sometimes the app wants to release an `EngineSession` to immediately render it on another `EngineView`; e.g. when transforming a Custom Tab into a regular browser tab.

* **browser-session**
  * ⚠️ **This is a breaking change**: Removed "default session" behavior from `SessionManager`. This feature was never used by any app except the sample browser.

* **feature-downloads**
  * Added `FetchDownloadManager`, an alternate download manager that uses a fetch `Client` instead of the native Android `DownloadManager`.

* **support-ktx**
  * Deprecated `String.toUri()` in favour of Android Core KTX.
  * Deprecated `View.isGone` and `View.isInvisible` in favour of Android Core KTX.
  * Added `putCompoundDrawablesRelative` and `putCompoundDrawablesRelativeWithIntrinsicBounds`, aliases of `setCompoundDrawablesRelative` that use Kotlin named and default arguments.

# 2.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v1.0.0...v2.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/61?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v2.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v2.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v2.0.0/buildSrc/src/main/java/Config.kt)


* **browser-toolbar**
  * Adds `focus()` which provides a hook for calling `editMode.focus()` to focus the edit mode `urlView`

* **browser-awesomebar**
  * Updated `DefaultSuggestionViewHolder` to have a style more consistent with Fenix mocks.
  * Fixed a bug with `InlineAutocompleteEditText` where the cursor would disappear if a user cleared an suggested URL.

* **lib-state**
  * A new component for maintaining application, screen or component state via a redux-style `Store`. This component provides the architectural foundation for the `browser-state` component (in development).

* **feature-downloads**
  * `onDownloadCompleted` no longer receives the download object and ID.

* **support-ktx**
  * Deprecated `Resource.pxToDp`.
  * Added `Int.dpToPx` to convert from density independent pixels to an int representing screen pixels.
  * Added `Int.dpToFloat` to convert from density independent pixels to a float representing screen pixels.

* **support-ktx**
  * Added `Context.isScreenReaderEnabled` extension to check if TalkBack service is enabled.

* **browser-icons**
  * The component now ships with the [tippy-top-sites](https://github.com/mozilla/tippy-top-sites) top 200 list for looking up icon resources.

* **concept-engine**, **browser-engine-gecko(-beta/nightly)**, **feature-session**, **feature-tabs**
  * Added to support for specifying additional flags when loading URLs. This can be done using the engine session directly, as well as via use cases:

  ```kotlin
  // Bypass cache
  sessionManager.getEngineSession().loadUrl(url, LoadUrlFlags.select(LoadUrlFlags.BYPASS_CACHE))

  // Bypass cache and proxy
  sessionUseCases.loadUrl.invoke(url, LoadUrlFlags.select(LoadUrlFlags.BYPASS_CACHE, LoadUrlFlags.BYPASS_PROXY))
  ```

# 1.0.0

* [Commits](https://github.com/mozilla-mobile/android-components/compare/v0.56.0...v1.0.0)
* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/60?closed=1)
* [Dependencies](https://github.com/mozilla-mobile/android-components/blob/v1.0.0/buildSrc/src/main/java/Dependencies.kt)
* [Gecko](https://github.com/mozilla-mobile/android-components/blob/v1.0.0/buildSrc/src/main/java/Gecko.kt)
* [Configuration](https://github.com/mozilla-mobile/android-components/blob/v1.0.0/buildSrc/src/main/java/Config.kt)

* 🛑 Removed deprecated components (See [blog posting](https://mozac.org/2019/05/23/deprecation.html)):
  * feature-session-bundling
  * ui-progress
  * ui-doorhanger

* **concept-engine**, **browser-engine-gecko(-beta/nightly)**, **browser-engine-system**
  * Added `Engine.version` property (`EngineVersion`) for printing and comparing the version of the used engine.

* **browser-menu**
  * Added `endOfMenuAlwaysVisible` property/parameter to `BrowserMenuBuilder` constructor and to `BrowserMenu.show` function.
    When is set to true makes sure the bottom of the menu is always visible, this allows use cases like [#3211](https://github.com/mozilla-mobile/android-components/issues/3211).
  * Added `onDimiss` parameter to `BrowserMenu.show` function, called when the menu is dismissed.
  * Changed `BrowserMenuHighlightableItem` constructor to allow for dynamically toggling the highlight with `invalidate()`.

* **browser-toolbar**
  * Added highlight effect to the overflow menu button when a highlighted `BrowserMenuHighlightableItem` is present.

* **feature-tab-collections**
  * Tabs can now be restored without restoring the ID of the `Session` by using the `restoreSessionId` flag. An app may
    prefer to use new IDs if it expects sessions to get restored multiple times - otherwise breaking the promise of a
    unique ID.

* **browser-search**
  * Added `getProvidedDefaultSearchEngine` to `SearchEngineManager` to return the provided default search engine or the first
    search engine if the default is not set. This allows use cases like [#3344](https://github.com/mozilla-mobile/android-components/issues/3344).

* **feature-tab-collections**
  * Behavior change: `TabCollection` instances returned by `TabCollectionStorage` are now ordered by the last time they have been updated (instead of the time they have been created).

* **lib-crash**
  * [Restrictions to background activity starts](https://developer.android.com/preview/privacy/background-activity-starts) in Android Q+ make it impossible to launch the crash reporter prompt after certain crashes. In those situations the library will show a "crash notification" instead. Clicking on the notification will launch the crash reporter prompt allowing the user to submit a crash report.

----

For older versions see the [changelog archive]({{ site.baseurl }}/changelog/archive).
