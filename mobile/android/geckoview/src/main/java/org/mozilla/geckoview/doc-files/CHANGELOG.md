---
layout: default
title: API Changelog
description: GeckoView API Changelog.
nav_exclude: true
exclude: true
---

{% capture javadoc_uri %}{{ site.url }}{{ site.baseurl}}/javadoc/mozilla-central/org/mozilla/geckoview{% endcapture %}

# GeckoView API Changelog.

## v70
- Added API for session context assignment
  [`GeckoSessionSettings.Builder.contextId`][70.1] and deletion of data
  related to a session context
  [`StorageController.clearDataForSessionContext`][70.2].
- Removed `setSession(session, runtime)` from [`GeckoView`][70.5]. With this change, `GeckoView` will no longer
  manage opening/closing of the [`GeckoSession`][70.6] and instead leave that up to the app. It's also now allowed
  to call [`setSession`][70.10] with a closed `GeckoSession`.
- Added an overload of [`GeckoSession.loadUri()`][70.8] that accepts a referring [`GeckoSession`][70.6]. This should be used
  when the URI we're loading originates from another page. A common example of this would be long pressing
  a link and then opening that in a new `GeckoSession`.
- Added capture parameter to [`onFilePrompt`][70.9] and corresponding [`CAPTURE_TYPE_*`][70.7] constants.
- Removed the obsolete `success` parameter from
  [`CrashReporter#sendCrashReport(Context, File, File, String)`][70.3] and
  [`CrashReporter#sendCrashReport(Context, File, Map, String)`][70.4].
- Add GeckoSession.LOAD_FLAGS_REPLACE_HISTORY
- Complete rewrite of [`PromptDelegate`][70.11].
- Added [`RuntimeTelemetry.Delegate`][70.12] that receives streaming telemetry
  data from GeckoView.
- Updated [`ContentBlocking`][70.13] to better report blocked and allowed ETP events.

[70.1]: {{javadoc_uri}}/GeckoSessionSettings.Builder.html#contextId-java.lang.String-
[70.2]: {{javadoc_uri}}/StorageController.html#clearDataForSessionContext-java.lang.String-
[70.3]: {{javadoc_uri}}/CrashReporter.html#sendCrashReport-android.content.Context-java.io.File-java.io.File-java.lang.String-
[70.4]: {{javadoc_uri}}/CrashReporter.html#sendCrashReport-android.content.Context-java.io.File-java.util.Map-java.lang.String-
[70.5]: {{javadoc_uri}}/GeckoView.html
[70.6]: {{javadoc_uri}}/GeckoSession.html
[70.7]: {{javadoc_uri}}/GeckoSession.PromptDelegate.html#CAPTURE_TYPE_NONE
[70.8]: {{javadoc_uri}}/GeckoSession.html#loadUri-java.lang.String-org.mozilla.geckoview.GeckoSession-int-
[70.9]: {{javadoc_uri}}/GeckoSession.PromptDelegate.html#onFilePrompt-org.mozilla.geckoview.GeckoSession-java.lang.String-int-java.lang.String:A-int-org.mozilla.geckoview.GeckoSession.PromptDelegate.FileCallback-
[70.10]: {{javadoc_uri}}/GeckoView.html#setSession-org.mozilla.geckoview.GeckoSession-
[70.11]: {{javadoc_uri}}/GeckoSession.PromptDelegate.html
[70.12]: {{javadoc_uri}}/RuntimeTelemetry.Delegate.html
[70.13]: {{javadoc_uri}}/ContentBlocking.html

## v69
- Modified behavior of ['setAutomaticFontSizeAdjustment'][69.1] so that it no 
  longer has any effect on ['setFontInflationEnabled'][69.2]
- Add [GeckoSession.LOAD_FLAGS_FORCE_ALLOW_DATA_URI][69.14]
- Added [`GeckoResult.accept`][69.3] for consuming a result without
  transforming it.
- [`GeckoSession.setMessageDelegate`][69.13] callers must now specify the
  [`WebExtension`][69.5] that the [`MessageDelegate`][69.4] will receive
  messages from.
- Added [`WebExtensionController`][69.9] and [`TabDelegate`][69.10] to handle
  [`browser.tabs.create`][69.6] calls by WebExtensions.
- Created [`onKill`][69.7] to [`ContentDelegate`][69.11] to differentiate from crashes.
- Added [`onCloseTab`][69.12] to [`WebExtensionController.TabDelegate`][69.10] to handle
  [`browser.tabs.remove`][69.8] calls by WebExtensions.

[69.1]: {{javadoc_uri}}/GeckoRuntimeSettings.html#setAutomaticFontSizeAdjustment-boolean-
[69.2]: {{javadoc_uri}}/GeckoRuntimeSettings.html#setFontInflationEnabled-boolean-
[69.3]: {{javadoc_uri}}/GeckoResult.html#accept-org.mozilla.geckoview.GeckoResult.Consumer-
[69.4]: {{javadoc_uri}}/WebExtension.MessageDelegate.html
[69.5]: {{javadoc_uri}}/WebExtension.html
[69.6]: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/tabs/create
[69.7]: {{javadoc_uri}}/GeckoSession.ContentDelegate.html#onKill-org.mozilla.geckoview.GeckoSession-
[69.8]: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/tabs/remove
[69.9]: {{javadoc_uri}}/WebExtensionController.html
[69.10]:{{javadoc_uri}}/WebExtensionController.TabDelegate.html
[69.11]: {{javadoc_uri}}/GeckoSession.ContentDelegate.html
[69.12]: {{javadoc_uri}}/WebExtensionController.TabDelegate.html#onCloseTab-org.mozilla.geckoview.WebExtension-org.mozilla.geckoview.GeckoSession-
[69.13]: {{javadoc_uri}}/GeckoSession.html#setMessageDelegate-org.mozilla.geckoview.WebExtension-org.mozilla.geckoview.WebExtension.MessageDelegate-java.lang.String-
[69.14]: {{javadoc_uri}}/GeckoSession.html#LOAD_FLAGS_FORCE_ALLOW_DATA_URI

## v68
- Added [`GeckoRuntime#configurationChanged`][68.1] to notify the device
  configuration has changed.
- Added [`onSessionStateChange`][68.29] to [`ProgressDelegate`][68.2] and removed `saveState`.
- Added [`ContentBlocking#AT_CRYPTOMINING`][68.3] for cryptocurrency miner blocking.
- Added [`ContentBlocking#AT_DEFAULT`][68.4], [`ContentBlocking#AT_STRICT`][68.5],
  [`ContentBlocking#CB_DEFAULT`][68.6] and [`ContentBlocking#CB_STRICT`][68.7]
  for clearer app default selections.
- Added [`GeckoSession.SessionState.fromString`][68.8]. This can be used to
  deserialize a `GeckoSession.SessionState` instance previously serialized to
  a `String` via `GeckoSession.SessionState.toString`.
- Added [`GeckoRuntimeSettings#setPreferredColorScheme`][68.9] to override
  the default color theme for web content ("light" or "dark").
- Added [`@NonNull`][66.1] or [`@Nullable`][66.2] to all fields.
- [`RuntimeTelemetry#getSnapshots`][68.10] returns a [`JSONObject`][68.30] now.
- Removed all `org.mozilla.gecko` references in the API.
- Added [`ContentBlocking#AT_FINGERPRINTING`][68.11] to block fingerprinting trackers.
- Added [`HistoryItem`][68.31] and [`HistoryList`][68.32] interfaces and [`onHistoryStateChange`][68.34] to 
  [`HistoryDelegate`][68.12] and added [`gotoHistoryIndex`][68.33] to [`GeckoSession`][68.13].
- [`GeckoView`][70.5] will not create a [`GeckoSession`][65.9] anymore when
  attached to a window without a session.
- Added [`GeckoRuntimeSettings.Builder#configFilePath`][68.16] to set
  a path to a configuration file from which GeckoView will read
  configuration options such as Gecko process arguments, environment
  variables, and preferences.
- Added [`unregisterWebExtension`][68.17] to unregister a web extension.
- Added messaging support for WebExtension. [`setMessageDelegate`][68.18]
  allows embedders to listen to messages coming from a WebExtension.
  [`Port`][68.19] allows bidirectional communication between the embedder and
  the WebExtension.
- Expose the following prefs in [`GeckoRuntimeSettings`][67.3]:
  [`setAutoZoomEnabled`][68.20], [`setDoubleTapZoomingEnabled`][68.21],
  [`setGlMsaaLevel`][68.22].
- Added new constant for requesting external storage Android permissions, [`PERMISSION_PERSISTENT_STORAGE`][68.35]
- Added `setVerticalClipping` to [`GeckoDisplay`][68.24] and
  [`GeckoView`][68.23] to tell Gecko how much of its vertical space is clipped.
- Added [`StorageController`][68.25] API for clearing data.
- Added [`onRecordingStatusChanged`][68.26] to [`MediaDelegate`][68.27] to handle events related to the status of recording devices.
- Removed redundant constants in [`MediaSource`][68.28]

[68.1]: {{javadoc_uri}}/GeckoRuntime.html#configurationChanged-android.content.res.Configuration-
[68.2]: {{javadoc_uri}}/GeckoSession.ProgressDelegate.html
[68.3]: {{javadoc_uri}}/ContentBlocking.html#AT_CRYPTOMINING
[68.4]: {{javadoc_uri}}/ContentBlocking.html#AT_DEFAULT
[68.5]: {{javadoc_uri}}/ContentBlocking.html#AT_STRICT
[68.6]: {{javadoc_uri}}/ContentBlocking.html#CB_DEFAULT
[68.7]: {{javadoc_uri}}/ContentBlocking.html#CB_STRICT
[68.8]: {{javadoc_uri}}/GeckoSession.SessionState.html#fromString-java.lang.String-
[68.9]: {{javadoc_uri}}/GeckoRuntimeSettings.html#setPreferredColorScheme-int-
[68.10]: {{javadoc_uri}}/RuntimeTelemetry.html#getSnapshots-boolean-
[68.11]: {{javadoc_uri}}/ContentBlocking.html#AT_FINGERPRINTING
[68.12]: {{javadoc_uri}}/GeckoSession.HistoryDelegate.html
[68.13]: {{javadoc_uri}}/GeckoSession.html
[68.16]: {{javadoc_uri}}/GeckoRuntimeSettings.Builder.html#configFilePath-java.lang.String-
[68.17]: {{javadoc_uri}}/GeckoRuntime.html#unregisterWebExtension-org.mozilla.geckoview.WebExtension-
[68.18]: {{javadoc_uri}}/WebExtension.html#setMessageDelegate-org.mozilla.geckoview.WebExtension.MessageDelegate-java.lang.String-
[68.19]: {{javadoc_uri}}/WebExtension.Port.html
[68.20]: {{javadoc_uri}}/GeckoRuntimeSettings.html#setAutoZoomEnabled-boolean-
[68.21]: {{javadoc_uri}}/GeckoRuntimeSettings.html#setDoubleTapZoomingEnabled-boolean-
[68.22]: {{javadoc_uri}}/GeckoRuntimeSettings.html#setGlMsaaLevel-int-
[68.23]: {{javadoc_uri}}/GeckoView.html#setVerticalClipping-int-
[68.24]: {{javadoc_uri}}/GeckoDisplay.html#setVerticalClipping-int-
[68.25]: {{javadoc_uri}}/StorageController.html
[68.26]: {{javadoc_uri}}/GeckoSession.MediaDelegate.html#onRecordingStatusChanged-org.mozilla.geckoview.GeckoSession-org.mozilla.geckoview.GeckoSession.MediaDelegate.RecordingDevice:A-
[68.27]: {{javadoc_uri}}/GeckoSession.MediaDelegate.html
[68.28]: {{javadoc_uri}}/GeckoSession.PermissionDelegate.MediaSource.html
[68.29]: {{javadoc_uri}}/GeckoSession.ProgressDelegate.html#onSessionStateChange-org.mozilla.geckoview.GeckoSession-org.mozilla.geckoview.GeckoSession.SessionState-
[68.30]: https://developer.android.com/reference/org/json/JSONObject
[68.31]: {{javadoc_uri}}/GeckoSession.HistoryDelegate.HistoryItem.html
[68.32]: {{javadoc_uri}}/GeckoSession.HistoryDelegate.HistoryList.html
[68.33]: {{javadoc_uri}}/GeckoSession.html#gotoHistoryIndex-int-
[68.34]: {{javadoc_uri}}/GeckoSession.HistoryDelegate.html#onHistoryStateChange-org.mozilla.geckoview.GeckoSession-org.mozilla.geckoview.GeckoSession.HistoryDelegate.HistoryList-
[68.35]: {{javadoc_uri}}/GeckoSession.PermissionDelegate.html#PERMISSION_PERSISTENT_STORAGE

## v67
- Added [`setAutomaticFontSizeAdjustment`][67.23] to
  [`GeckoRuntimeSettings`][67.3] for automatically adjusting font size settings
  depending on the OS-level font size setting.
- Added [`setFontSizeFactor`][67.4] to [`GeckoRuntimeSettings`][67.3] for
  setting a font size scaling factor, and for enabling font inflation for
  non-mobile-friendly pages.
- Updated video autoplay API to reflect changes in Gecko. Instead of being a
  per-video permission in the [`PermissionDelegate`][67.5], it is a [runtime
  setting][67.6] that either allows or blocks autoplay videos.
- Change [`ContentBlocking.AT_AD`][67.7] and [`ContentBlocking.SB_ALL`][67.8]
  values to mirror the actual constants they encompass.
- Added nested [`ContentBlocking`][67.9] runtime settings.
- Added [`RuntimeSettings`][67.10] base class to support nested settings.
- Added [`baseUri`][67.11] to [`ContentDelegate.ContextElement`][65.21] and
  changed [`linkUri`][67.12] to absolute form.
- Added [`scrollBy`][67.13] and [`scrollTo`][67.14] to [`PanZoomController`][65.4].
- Added [`GeckoSession.getDefaultUserAgent`][67.1] to expose the build-time
  default user agent synchronously.
- Changed [`WebResponse.body`][67.24] from a [`ByteBuffer`][67.25] to an [`InputStream`][67.26]. Apps that want access
  to the entire response body will now need to read the stream themselves.
- Added [`GeckoWebExecutor.FETCH_FLAGS_NO_REDIRECTS`][67.27], which will cause [`GeckoWebExecutor.fetch()`][67.28] to not
  automatically follow [HTTP redirects][67.29] (e.g., 302).
- Moved [`GeckoVRManager`][67.2] into the org.mozilla.geckoview package.
- Initial WebExtension support. [`GeckoRuntime#registerWebExtension`][67.15]
  allows embedders to register a local web extension.
- Added API to [`GeckoView`][70.5] to take screenshot of the visible page. Calling [`capturePixels`][67.16] returns a ['GeckoResult'][65.25] that completes to a [`Bitmap`][67.17] of the current [`Surface`][67.18] contents, or an [`IllegalStateException`][67.19] if the [`GeckoSession`][65.9] is not ready to render content.
- Added API to capture a screenshot to [`GeckoDisplay`][67.20]. [`capturePixels`][67.21] returns a ['GeckoResult'][65.25] that completes to a [`Bitmap`][67.16] of the current [`Surface`][67.17] contents, or an [`IllegalStateException`][67.18] if the [`GeckoSession`][65.9] is not ready to render content.
- Add missing [`@Nullable`][66.2] annotation to return value for
  [`GeckoSession.PromptDelegate.ChoiceCallback.onPopupResult()`][67.30]
- Added `default` implementations for all non-functional `interface`s.
- Added [`ContentDelegate.onWebAppManifest`][67.22], which will deliver the contents of a parsed
  and validated Web App Manifest on pages that contain one.

[67.1]: {{javadoc_uri}}/GeckoSession.html#getDefaultUserAgent--
[67.2]: {{javadoc_uri}}/GeckoVRManager.html
[67.3]: {{javadoc_uri}}/GeckoRuntimeSettings.html
[67.4]: {{javadoc_uri}}/GeckoRuntimeSettings.html#setFontSizeFactor-float-
[67.5]: {{javadoc_uri}}/GeckoSession.PermissionDelegate.html
[67.6]: {{javadoc_uri}}/GeckoRuntimeSettings.html#setAutoplayDefault-int-
[67.7]: {{javadoc_uri}}/ContentBlocking.html#AT_AD
[67.8]: {{javadoc_uri}}/ContentBlocking.html#SB_ALL
[67.9]: {{javadoc_uri}}/ContentBlocking.html
[67.10]: {{javadoc_uri}}/RuntimeSettings.html
[67.11]: {{javadoc_uri}}/GeckoSession.ContentDelegate.ContextElement.html#baseUri
[67.12]: {{javadoc_uri}}/GeckoSession.ContentDelegate.ContextElement.html#linkUri
[67.13]: {{javadoc_uri}}/PanZoomController.html#scrollBy-org.mozilla.geckoview.ScreenLength-org.mozilla.geckoview.ScreenLength-
[67.14]: {{javadoc_uri}}/PanZoomController.html#scrollTo-org.mozilla.geckoview.ScreenLength-org.mozilla.geckoview.ScreenLength-
[67.15]: {{javadoc_uri}}/GeckoRuntime.html#registerWebExtension-org.mozilla.geckoview.WebExtension-
[67.16]: {{javadoc_uri}}/GeckoView.html#capturePixels--
[67.17]: https://developer.android.com/reference/android/graphics/Bitmap
[67.18]: https://developer.android.com/reference/android/view/Surface
[67.19]: https://developer.android.com/reference/java/lang/IllegalStateException
[67.20]: {{javadoc_uri}}/GeckoDisplay.html
[67.21]: {{javadoc_uri}}/GeckoDisplay.html#capturePixels--
[67.22]: {{javadoc_uri}}/GeckoSession.ContentDelegate.html#onWebAppManifest-org.mozilla.geckoview.GeckoSession-org.json.JSONObject-
[67.23]: {{javadoc_uri}}/GeckoRuntimeSettings.html#setAutomaticFontSizeAdjustment-boolean-
[67.24]: {{javadoc_uri}}/WebResponse.html#body
[67.25]: https://developer.android.com/reference/java/nio/ByteBuffer
[67.26]: https://developer.android.com/reference/java/io/InputStream
[67.27]: {{javadoc_uri}}/GeckoWebExecutor.html#FETCH_FLAGS_NO_REDIRECTS
[67.28]: {{javadoc_uri}}/GeckoWebExecutor.html#fetch-org.mozilla.geckoview.WebRequest-int-
[67.29]: https://developer.mozilla.org/en-US/docs/Web/HTTP/Redirections
[67.30]: {{javadoc_uri}}/GeckoSession.PromptDelegate.ChoiceCallback.html

## v66
- Removed redundant field `trackingMode` from [`SecurityInformation`][66.6].
  Use `TrackingProtectionDelegate.onTrackerBlocked` for notification of blocked
  elements during page load.
- Added [`@NonNull`][66.1] or [`@Nullable`][66.2] to all APIs.
- Added methods for each setting in [`GeckoSessionSettings`][66.3]
- Added [`GeckoSessionSettings`][66.4] for enabling desktop viewport. Desktop
  viewport is no longer set by [`USER_AGENT_MODE_DESKTOP`][66.5] and must be set
  separately.
- Added [`@UiThread`][65.6] to [`GeckoSession.releaseSession`][66.7] and
  [`GeckoSession.setSession`][66.8]

[66.1]: https://developer.android.com/reference/android/support/annotation/NonNull
[66.2]: https://developer.android.com/reference/android/support/annotation/Nullable
[66.3]: {{javadoc_uri}}/GeckoSessionSettings.html
[66.4]: {{javadoc_uri}}/GeckoSessionSettings.html
[66.5]: {{javadoc_uri}}/GeckoSessionSettings.html#USER_AGENT_MODE_DESKTOP
[66.6]: {{javadoc_uri}}/GeckoSession.ProgressDelegate.SecurityInformation.html
[66.7]: {{javadoc_uri}}/GeckoView.html#releaseSession--
[66.8]: {{javadoc_uri}}/GeckoView.html#setSession-org.mozilla.geckoview.GeckoSession-

## v65
- Added experimental ad-blocking category to `GeckoSession.TrackingProtectionDelegate`.
- Moved [`CompositorController`][65.1], [`DynamicToolbarAnimator`][65.2],
  [`OverscrollEdgeEffect`][65.3], [`PanZoomController`][65.4] from
  `org.mozilla.gecko.gfx` to [`org.mozilla.geckoview`][65.5]
- Added [`@UiThread`][65.6], [`@AnyThread`][65.7] annotations to all APIs
- Changed `GeckoRuntimeSettings#getLocale` to [`getLocales`][65.8] and related
  APIs.
- Merged `org.mozilla.gecko.gfx.LayerSession` into [`GeckoSession`][65.9]
- Added [`GeckoSession.MediaDelegate`][65.10] and [`MediaElement`][65.11]. This
  allow monitoring and control of web media elements (play, pause, seek, etc).
- Removed unused `access` parameter from
  [`GeckoSession.PermissionDelegate#onContentPermissionRequest`][65.12]
- Added [`WebMessage`][65.13], [`WebRequest`][65.14], [`WebResponse`][65.15],
  and [`GeckoWebExecutor`][65.16]. This exposes Gecko networking to apps. It
  includes speculative connections, name resolution, and a Fetch-like HTTP API.
- Added [`GeckoSession.HistoryDelegate`][65.17]. This allows apps to implement
  their own history storage system and provide visited link status.
- Added [`ContentDelegate#onFirstComposite`][65.18] to get first composite
  callback after a compositor start.
- Changed `LoadRequest.isUserTriggered` to [`isRedirect`][65.19].
- Added [`GeckoSession.LOAD_FLAGS_BYPASS_CLASSIFIER`][65.20] to bypass the URI
  classifier.
- Added a `protected` empty constructor to all field-only classes so that apps
  can mock these classes in tests.
- Added [`ContentDelegate.ContextElement`][65.21] to extend the information
  passed to [`ContentDelegate#onContextMenu`][65.22]. Extended information
  includes the element's title and alt attributes.
- Changed [`ContentDelegate.ContextElement`][65.21] `TYPE_` constants to public
  access.
- Changed [`ContentDelegate.ContextElement`][65.21],
  [`GeckoSession.FinderResult`][65.23] to non-final class.
- Update [`CrashReporter#sendCrashReport`][65.24] to return the crash ID as a
  [`GeckoResult<String>`][65.25].

[65.1]: {{javadoc_uri}}/CompositorController.html
[65.2]: {{javadoc_uri}}/DynamicToolbarAnimator.html
[65.3]: {{javadoc_uri}}/OverscrollEdgeEffect.html
[65.4]: {{javadoc_uri}}/PanZoomController.html
[65.5]: {{javadoc_uri}}/package-summary.html
[65.6]: https://developer.android.com/reference/android/support/annotation/UiThread
[65.7]: https://developer.android.com/reference/android/support/annotation/AnyThread
[65.8]: {{javadoc_uri}}/GeckoRuntimeSettings.html#getLocales--
[65.9]: {{javadoc_uri}}/GeckoSession.html
[65.10]: {{javadoc_uri}}/GeckoSession.MediaDelegate.html
[65.11]: {{javadoc_uri}}/MediaElement.html
[65.12]: {{javadoc_uri}}/GeckoSession.PermissionDelegate.html#onContentPermissionRequest-org.mozilla.geckoview.GeckoSession-java.lang.String-int-org.mozilla.geckoview.GeckoSession.PermissionDelegate.Callback-
[65.13]: {{javadoc_uri}}/WebMessage.html
[65.14]: {{javadoc_uri}}/WebRequest.html
[65.15]: {{javadoc_uri}}/WebResponse.html
[65.16]: {{javadoc_uri}}/GeckoWebExecutor.html
[65.17]: {{javadoc_uri}}/GeckoSession.HistoryDelegate.html    
[65.18]: {{javadoc_uri}}/GeckoSession.ContentDelegate.html#onFirstComposite-org.mozilla.geckoview.GeckoSession-
[65.19]: {{javadoc_uri}}/GeckoSession.NavigationDelegate.LoadRequest.html#isRedirect
[65.20]: {{javadoc_uri}}/GeckoSession.html#LOAD_FLAGS_BYPASS_CLASSIFIER    
[65.21]: {{javadoc_uri}}/GeckoSession.ContentDelegate.ContextElement.html
[65.22]: {{javadoc_uri}}/GeckoSession.ContentDelegate.html#onContextMenu-org.mozilla.geckoview.GeckoSession-int-int-org.mozilla.geckoview.GeckoSession.ContentDelegate.ContextElement-
[65.23]: {{javadoc_uri}}/GeckoSession.FinderResult.html
[65.24]: {{javadoc_uri}}/CrashReporter.html#sendCrashReport-android.content.Context-android.os.Bundle-java.lang.String-
[65.25]: {{javadoc_uri}}/GeckoResult.html

[api-version]: 1929f5739623cec70ce9bad19837a4f21604488e
