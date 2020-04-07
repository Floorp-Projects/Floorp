---
layout: default
title: API Changelog
description: GeckoView API Changelog.
nav_exclude: true
exclude: true
---

{% capture javadoc_uri %}{{ site.url }}{{ site.baseurl}}/javadoc/mozilla-central/org/mozilla/geckoview{% endcapture %}
{% capture bugzilla %}https://bugzilla.mozilla.org/show_bug.cgi?id={% endcapture %}

# GeckoView API Changelog.

⚠️  breaking change

## v77
- Added [`GeckoRuntime.appendAppNotesToCrashReport`][77.1] For adding app notes to the crash report.
  ([bug 1626979]({{bugzilla}}1626979))

[77.1]: {{javadoc_uri}}/GeckoRuntime.html#appendAppNotesToCrashReport-java.lang.String-

## v76
- Added [`GeckoSession.PermissionDelegate.PERMISSION_MEDIA_KEY_SYSTEM_ACCESS`][76.1] to control EME media key access.
- [`RuntimeTelemetry#getSnapshots`][68.10] is deprecated and will be removed
  in 79. Use Glean to handle Gecko telemetry.
  ([bug 1620395]({{bugzilla}}1620395))
- Added [`LoadRequest.isDirectNavigation`] to know when calls to
  [`onLoadRequest`][76.3] originate from a direct navigation made by the app
  itself.
  ([bug 1624675]({{bugzilla}}1624675))

[76.1]: {{javadoc_uri}}/GeckoSession.PermissionDelegate.html#PERMISSION_MEDIA_KEY_SYSTEM_ACCESS
[76.2]: {{javadoc_uri}}/GeckoSession.NavigationDelegate.LoadRequest.html#isDirectNavigation
[76.3]: {{javadoc_uri}}/GeckoSession.NavigationDelegate.html#onLoadRequest-org.mozilla.geckoview.GeckoSession-org.mozilla.geckoview.GeckoSession.NavigationDelegate.LoadRequest-

## v75
- ⚠️ Remove `GeckoRuntimeSettings.Builder#useContentProcessHint`. The content
  process is now preloaded by default if
  [`GeckoRuntimeSettings.Builder#useMultiprocess`][75.1] is enabled.
- ⚠️ Move `GeckoSessionSettings.Builder#useMultiprocess` to
  [`GeckoRuntimeSettings.Builder#useMultiprocess`][75.1]. Multiprocess state is
  no longer determined per session.
- Added [`DebuggerDelegate#onExtensionListUpdated`][75.2] to notify that a temporary
  extension has been installed by the debugger.
  ([bug 1614295]({{bugzilla}}1614295))
- ⚠️ Removed [`GeckoRuntimeSettings.setAutoplayDefault`][75.3], use
  [`GeckoSession.PermissionDelegate#PERMISSION_AUTOPLAY_AUDIBLE`][73.12] and
  [`GeckoSession.PermissionDelegate#PERMISSION_AUTOPLAY_INAUDIBLE`][73.13] to
  control autoplay.
  ([bug 1614894]({{bugzilla}}1614894))
- Added [`GeckoSession.reload(int flags)`][75.4] That takes a [load flag][75.5] parameter.
- ⚠️ Moved [`ActionDelegate`][75.6] and [`MessageDelegate`][75.7] to
  [`SessionController`][75.8].
  ([bug 1616625]({{bugzilla}}1616625))
- Added [`SessionTabDelegate`][75.9] to [`SessionController`][75.8] and
  [`TabDelegate`][75.10] to [`WebExtension`][69.5] which receive respectively
  calls for the session and the runtime. `TabDelegate` is also now
  per-`WebExtension` object instead of being global.  The existing global
  [`TabDelegate`][75.11] is now deprecated and will be removed in GeckoView 77.
  ([bug 1616625]({{bugzilla}}1616625))
- Added [`SessionTabDelegate#onUpdateTab`][75.12] which is called whenever an
  extension calls `tabs.update` on the corresponding `GeckoSession`.
  [`TabDelegate#onCreateTab`][75.13] now takes a [`CreateTabDetails`][75.14]
  object which contains additional information about the newly created tab
  (including the `url` which used to be passed in directly).
  ([bug 1616625]({{bugzilla}}1616625))
- Added [`GeckoRuntimeSettings.setWebManifestEnabled`][75.15],
  [`GeckoRuntimeSettings.webManifest`][75.16], and
  [`GeckoRuntimeSettings.getWebManifestEnabled`][75.17]
  ([bug 1614894]({{bugzilla}}1603673)), to enable or check Web Manifest support.
- Added [`GeckoDisplay.safeAreaInsetsChanged`][75.18] to notify the content of [safe area insets][75.19].
  ([bug 1503656]({{bugzilla}}1503656))
- Added [`GeckoResult#cancel()`][75.22], [`GeckoResult#setCancellationDelegate()`][75.22],
  and [`GeckoResult.CancellationDelegate`][75.23]. This adds the optional ability to cancel
  an operation behind a pending `GeckoResult`.
- Added [`baseUrl`][75.24] to [`WebExtension.MetaData`][75.25] to expose the
  base URL for all WebExtension pages for a given extension.
  ([bug 1560048]({{bugzilla}}1560048))
- Added [`allowedInPrivateBrowsing`][75.26] and
  [`setAllowedInPrivateBrowsing`][75.27] to control whether an extension can
  run in private browsing or not.  Extensions installed with
  [`registerWebExtension`][67.15] will always be allowed to run in private
  browsing.
  ([bug 1599139]({{bugzilla}}1599139))

[75.1]: {{javadoc_uri}}/GeckoRuntimeSettings.Builder.html#useMultiprocess-boolean-
[75.2]: {{javadoc_uri}}/WebExtensionController.DebuggerDelegate.html#onExtensionListUpdated--
[75.3]: {{javadoc_uri}}/GeckoRuntimeSettings.Builder.html#autoplayDefault-boolean-
[75.4]: {{javadoc_uri}}/GeckoSession.html#reload-int-
[75.5]: {{javadoc_uri}}/GeckoSession.html#LOAD_FLAGS_NONE
[75.6]: {{javadoc_uri}}/WebExtension.ActionDelegate.html
[75.7]: {{javadoc_uri}}/WebExtension.MessageDelegate.html
[75.8]: {{javadoc_uri}}/WebExtension.SessionController.html
[75.9]: {{javadoc_uri}}/WebExtension.SessionTabDelegate.html
[75.10]: {{javadoc_uri}}/WebExtension.TabDelegate.html
[75.11]: {{javadoc_uri}}/WebExtensionRuntime.TabDelegate.html
[75.12]: {{javadoc_uri}}/WebExtension.SessionTabDelegate.html#onUpdateTab-org.mozilla.geckoview.WebExtension-org.mozilla.geckoview.GeckoSession-org.mozilla.geckoview.WebExtension.UpdateTabDetails-
[75.13]: {{javadoc_uri}}/WebExtension.TabDelegate.html#onNewTab-org.mozilla.geckoview.WebExtension-org.mozilla.geckoview.WebExtension.CreateTabDetails-
[75.14]: {{javadoc_uri}}/WebExtension.CreateTabDetails.html
[75.15]: {{javadoc_uri}}/GeckoRuntimeSettings.Builder.html#setWebManifestEnabled-boolean-
[75.16]: {{javadoc_uri}}/GeckoRuntimeSettings.Builder.html#webManifest-boolean-
[75.17]: {{javadoc_uri}}/GeckoRuntimeSettings.Builder.html#getWebManifestEnabled--
[75.18]: {{javadoc_uri}}/GeckoDisplay.html#safeAreaInsetsChanged-int-int-int-int-
[75.19]: https://developer.mozilla.org/en-US/docs/Web/CSS/env
[75.20]: {{javadoc_uri}}/WebExtension.InstallException.ErrorCodes.html#ERROR_POSTPONED-
[75.21]: {{javadoc_uri}}/GeckoResult.html#cancel--
[75.22]: {{javadoc_uri}}/GeckoResult.html#setCancellationDelegate-CancellationDelegate-
[75.23]: {{javadoc_uri}}/GeckoResult.CancellationDelegate.html
[75.24]: {{javadoc_uri}}/WebExtension.MetaData.html#baseUrl
[75.25]: {{javadoc_uri}}/WebExtension.MetaData.html
[75.26]: {{javadoc_uri}}/WebExtension.MetaData.html#allowedInPrivateBrowsing
[75.27]: {{javadoc_uri}}/WebExtensionController.html#setAllowedInPrivateBrowsing-org.mozilla.geckoview.WebExtension-boolean-

## v74
- Added [`WebExtensionController.enable`][74.1] and [`disable`][74.2] to
  enable and disable extensions.
  ([bug 1599585]({{bugzilla}}1599585))
- ⚠️ Added ['GeckoSession.ProgressDelegate.SecurityInformation#certificate'][74.3], which is the
  full server certificate in use, if any. The other certificate-related fields were removed.
  ([bug 1508730]({{bugzilla}}1508730))
- Added ['WebResponse#isSecure'][74.4], which indicates whether or not the response was
  delivered over a secure connection.
  ([bug 1508730]({{bugzilla}}1508730))
- Added ['WebResponse#certificate'][74.5], which is the server certificate used for the
  response, if any.
  ([bug 1508730]({{bugzilla}}1508730))
- Added ['WebRequestError#certificate'][74.6], which is the server certificate used in the
  failed request, if any.
  ([bug 1508730]({{bugzilla}}1508730))
- ⚠️ Updated [`ContentBlockingController`][74.7] to use new representation for content blocking
  exceptions and to add better support for removing exceptions. This deprecates [`ExceptionList`][74.8]
  and [`restoreExceptionList`][74.9] with the intent to remove them in 76.
  ([bug 1587552]({{bugzilla}}1587552))
- Added [`GeckoSession.ContentDelegate.onMetaViewportFitChange`][74.10]. This exposes `viewport-fit` value that is CSS Round Display Level 1. ([bug 1574307]({{bugzilla}}1574307))
- Extended [`LoginStorage.Delegate`][74.11] with [`onLoginUsed`][74.12] to
  report when existing login entries are used for autofill.
  ([bug 1610353]({{bugzilla}}1610353))
- Added ['WebExtensionController#setTabActive'][74.13], which is used to notify extensions about
  tab changes
  ([bug 1597793]({{bugzilla}}1597793))
- Added ['WebExtension.metaData.optionsUrl'][74.14] and ['WebExtension.metaData.openOptionsPageInTab'][74.15],
  which is the addon metadata necessary to show their option pages.
  ([bug 1598792]({{bugzilla}}1598792))
- Added [`WebExtensionController.update`][74.16] to update extensions. ([bug 1599581]({{bugzilla}}1599581))
- ⚠️ Replaced `subscription` argument in [`WebPushDelegate.onSubscriptionChanged`][74.17] from a [`WebPushSubscription`][74.18] to the [`String`][74.19] `scope`.

[74.1]: {{javadoc_uri}}/WebExtensionController.html#enable-org.mozilla.geckoview.WebExtension-int-
[74.2]: {{javadoc_uri}}/WebExtensionController.html#disable-org.mozilla.geckoview.WebExtension-int-
[74.3]: {{javadoc_uri}}/GeckoSession.ProgressDelegate.SecurityInformation.html#certificate
[74.4]: {{javadoc_uri}}/WebResponse.html#isSecure
[74.5]: {{javadoc_uri}}/WebResponse.html#certificate
[74.6]: {{javadoc_uri}}/WebRequestError.html#certificate
[74.7]: {{javadoc_uri}}/ContentBlockingController.html
[74.8]: {{javadoc_uri}}/ContentBlockingController.ExceptionList.html
[74.9]: {{javadoc_uri}}/ContentBlockingController.html#restoreExceptionList-org.mozilla.geckoview.ContentBlockingController.ExceptionList-
[74.10]: {{javadoc_uri}}/GeckoSession.ContentDelegate.html#onMetaViewportFitChange-org.mozilla.geckoview.GeckoSession-java.lang.String-
[74.11]: {{javadoc_uri}}/LoginStorage.Delegate.html
[74.12]: {{javadoc_uri}}/LoginStorage.Delegate.html#onLoginUsed-org.mozilla.geckoview.LoginStorage.LoginEntry-int-
[74.13]: {{javadoc_uri}}/WebExtensionController.html#setTabActive
[74.14]: {{javadoc_uri}}/WebExtension.MetaData.html#optionsUrl
[74.15]: {{javadoc_uri}}/WebExtension.MetaData.html#openOptionsPageInTab
[74.16]: {{javadoc_uri}}/WebExtensionController.html#update-org.mozilla.geckoview.WebExtension-int-
[74.17]: {{javadoc_uri}}/WebPushController.html#onSubscriptionChange-org.mozilla.geckoview.WebPushSubscription-byte:A-
[74.18]: {{javadoc_uri}}/WebPushSubscription.html
[74.19]: https://developer.android.com/reference/java/lang/String

## v73
- Added [`WebExtensionController.install`][73.1] and [`uninstall`][73.2] to
  manage installed extensions
- ⚠️ Renamed `ScreenLength.VIEWPORT_WIDTH`, `ScreenLength.VIEWPORT_HEIGHT`,
  `ScreenLength.fromViewportWidth` and `ScreenLength.fromViewportHeight` to
  [`ScreenLength.VISUAL_VIEWPORT_WIDTH`][73.3],
  [`ScreenLength.VISUAL_VIEWPORT_HEIGHT`][73.4],
  [`ScreenLength.fromVisualViewportWidth`][73.5] and
  [`ScreenLength.fromVisualViewportHeight`][73.6] respectively.
- Added the [`LoginStorage`][73.7] API. Apps may handle login fetch requests now by
  attaching a [`LoginStorage.Delegate`][73.8] via
  [`GeckoRuntime#setLoginStorageDelegate`][73.9]
  ([bug 1602881]({{bugzilla}}1602881))
- ⚠️ [`WebExtension`][69.5]'s constructor now requires a `WebExtensionController`
  instance.
- Added [`GeckoResult.allOf`][73.10] for consuming a list of results.
- Added [`WebExtensionController.list`][73.11] to list all installed extensions.
- Added [`GeckoSession.PermissionDelegate#PERMISSION_AUTOPLAY_AUDIBLE`][73.12] and
  [`GeckoSession.PermissionDelegate#PERMISSION_AUTOPLAY_INAUDIBLE`][73.13]. These control
  autoplay permissions for audible and inaudible videos.
  ([bug 1577596]({{bugzilla}}1577596))
- Added [`LoginStorage.Delegate.onLoginSave`][73.14] for login storage save
  requests and [`GeckoSession.PromptDelegate.onLoginStoragePrompt`][73.15] for
  login storage prompts.
  ([bug 1599873]({{bugzilla}}1599873))

[73.1]: {{javadoc_uri}}/WebExtensionController.html#install-java.lang.String-
[73.2]: {{javadoc_uri}}/WebExtensionController.html#uninstall-org.mozilla.geckoview.WebExtension-
[73.3]: {{javadoc_uri}}/ScreenLength.html#VISUAL_VIEWPORT_WIDTH
[73.4]: {{javadoc_uri}}/ScreenLength.html#VISUAL_VIEWPORT_HEIGHT
[73.5]: {{javadoc_uri}}/ScreenLength.html#fromVisualViewportWidth-double-
[73.6]: {{javadoc_uri}}/ScreenLength.html#fromVisualViewportHeight-double-
[73.7]: {{javadoc_uri}}/LoginStorage.html
[73.8]: {{javadoc_uri}}/LoginStorage.Delegate.html
[73.9]: {{javadoc_uri}}/GeckoRuntime.html#setLoginStorageDelegate-org.mozilla.geckoview.LoginStorage.Delegate-
[73.10]: {{javadoc_uri}}/GeckoResult.html#allOf-java.util.List-
[73.11]: {{javadoc_uri}}/WebExtensionController.html#list--
[73.12]: {{javadoc_uri}}/GeckoSession.PermissionDelegate.html#PERMISSION_AUTOPLAY_AUDIBLE
[73.13]: {{javadoc_uri}}/GeckoSession.PermissionDelegate.html#PERMISSION_AUTOPLAY_INAUDIBLE
[73.14]: {{javadoc_uri}}/LoginStorage.Delegate.html#onLoginSave-org.mozilla.geckoview.LoginStorage.LoginEntry-
[73.15]: {{javadoc_uri}}/GeckoSession.PromptDelegate.html#onLoginStoragePrompt-org.mozilla.geckoview.GeckoSession-org.mozilla.geckoview.GeckoSession.PromptDelegate.LoginStoragePrompt-

## v72
- Added [`GeckoSession.NavigationDelegate.LoadRequest#hasUserGesture`][72.1]. This indicates
  if a load was requested while a user gesture was active (e.g., a tap).
  ([bug 1555337]({{bugzilla}}1555337))
- ⚠️  Refactored `AutofillElement` and `AutofillSupport` into the
  [`Autofill`][72.2] API.
  ([bug 1591462]({{bugzilla}}1591462))
- Make `read()` in the `InputStream` returned from [`WebResponse#body`][72.3] timeout according
  to [`WebResponse#setReadTimeoutMillis()`][72.4]. The default timeout value is reflected in
  [`WebResponse#DEFAULT_READ_TIMEOUT_MS`][72.5], currently 30s.
  ([bug 1595145]({{bugzilla}}1595145))
- ⚠️  Removed `GeckoResponse`
  ([bug 1581161]({{bugzilla}}1581161))
- ⚠️  Removed `actions` and `response` arguments from [`SelectionActionDelegate.onShowActionRequest`][72.6]
  and [`BasicSelectionActionDelegate.onShowActionRequest`][72.7]
  ([bug 1581161]({{bugzilla}}1581161))
- Added text selection action methods to [`SelectionActionDelegate.Selection`][72.8]
  ([bug 1581161]({{bugzilla}}1581161))
- Added [`BasicSelectionActionDelegate.getSelection`][72.9]
  ([bug 1581161]({{bugzilla}}1581161))
- Changed [`BasicSelectionActionDelegate.clearSelection`][72.10] to public.
  ([bug 1581161]({{bugzilla}}1581161))
- Added `Autofill` commit support.
  ([bug 1577005]({{bugzilla}}1577005))
- Added [`GeckoView.setViewBackend`][72.11] to set whether GeckoView should be
  backed by a [`TextureView`][72.12] or a [`SurfaceView`][72.13].
  ([bug 1530402]({{bugzilla}}1530402))
- Added support for Browser and Page Action from the WebExtension API.
  See [`WebExtension.Action`][72.14].
  ([bug 1530402]({{bugzilla}}1530402))
- ⚠️ Split [`ContentBlockingController.Event.LOADED_TRACKING_CONTENT`][72.15] into
  [`ContentBlockingController.Event.LOADED_LEVEL_1_TRACKING_CONTENT`][72.16] and
  [`ContentBlockingController.Event.LOADED_LEVEL_2_TRACKING_CONTENT`][72.17].
- Replaced `subscription` argument in [`WebPushDelegate.onPushEvent`][72.18] from a [`WebPushSubscription`][72.19] to the [`String`][72.20] `scope`.
- ⚠️ Renamed `WebExtension.ActionIcon` to [`Icon`][72.21].
- Added ['GeckoWebExecutor#FETCH_FLAGS_STREAM_FAILURE_TEST'][72.22], which is a new
  flag used to immediately fail when reading a `WebResponse` body.
  ([bug 1594905]({{bugzilla}}1594905))
- Changed [`CrashReporter#sendCrashReport(Context, File, JSONObject)`][72.23] to
  accept a JSON object instead of a Map. Said object also includes the
  application name that was previously passed as the fourth argument to the
  method, which was thus removed.
- Added WebXR device access permission support, [`PERMISSION_PERSISTENT_XR`][72.24].
  ([bug 1599927]({{bugzilla}}1599927))

[72.1]: {{javadoc_uri}}/GeckoSession.NavigationDelegate.LoadRequest#hasUserGesture-
[72.2]: {{javadoc_uri}}/Autofill.html
[72.3]: {{javadoc_uri}}/WebResponse.html#body
[72.4]: {{javadoc_uri}}/WebResponse.html#setReadTimeoutMillis-long-
[72.5]: {{javadoc_uri}}/WebResponse.html#DEFAULT_READ_TIMEOUT_MS
[72.6]: {{javadoc_uri}}/GeckoSession.SelectionActionDelegate.html#onShowActionRequest-org.mozilla.geckoview.GeckoSession-org.mozilla.geckoview.GeckoSession.SelectionActionDelegate.Selection-
[72.7]: {{javadoc_uri}}/BasicSelectionActionDelegate.html#onShowActionRequest-org.mozilla.geckoview.GeckoSession-org.mozilla.geckoview.GeckoSession.SelectionActionDelegate.Selection-
[72.8]: {{javadoc_uri}}/GeckoSession.SelectionActionDelegate.Selection.html
[72.9]: {{javadoc_uri}}/BasicSelectionActionDelegate.html#getSelection-
[72.10]: {{javadoc_uri}}/BasicSelectionActionDelegate.html#clearSelection-
[72.11]: {{javadoc_uri}}/GeckoView.html#setViewBackend-int-
[72.12]: https://developer.android.com/reference/android/view/TextureView
[72.13]: https://developer.android.com/reference/android/view/SurfaceView
[72.14]: {{javadoc_uri}}/WebExtension.Action.html
[72.15]: {{javadoc_uri}}/ContentBlockingController.Event.html#LOADED_TRACKING_CONTENT
[72.16]: {{javadoc_uri}}/ContentBlockingController.Event.html#LOADED_LEVEL_1_TRACKING_CONTENT
[72.17]: {{javadoc_uri}}/ContentBlockingController.Event.html#LOADED_LEVEL_2_TRACKING_CONTENT
[72.18]: {{javadoc_uri}}/WebPushController.html#onPushEvent-org.mozilla.geckoview.WebPushSubscription-byte:A-
[72.19]: {{javadoc_uri}}/WebPushSubscription.html
[72.20]: https://developer.android.com/reference/java/lang/String
[72.21]: {{javadoc_uri}}/WebExtension.Icon.html
[72.22]: {{javadoc_uri}}/GeckoWebExecutor.html#FETCH_FLAGS_STREAM_FAILURE_TEST
[72.23]: {{javadoc_uri}}/CrashReporter.html#sendCrashReport-android.content.Context-java.io.File-org.json.JSONObject-
[72.24]: {{javadoc_uri}}/GeckoSession.PermissionDelegate.html#PERMISSION_PERSISTENT_XR

=
## v71
- Added a content blocking flag for blocked social cookies to [`ContentBlocking`][70.17].
  ([bug 1584479]({{bugzilla}}1584479))
- Added [`onBooleanScalar`][71.1], [`onLongScalar`][71.2],
  [`onStringScalar`][71.3] to [`RuntimeTelemetry.Delegate`][70.12] to support
  scalars in streaming telemetry. ⚠️  As part of this change,
  `onTelemetryReceived` has been renamed to [`onHistogram`][71.4], and
  [`Metric`][71.5] now takes a type parameter.
  ([bug 1576730]({{bugzilla}}1576730))
- Added overloads of [`GeckoSession.loadUri`][71.6] that accept a map of
  additional HTTP request headers.
  ([bug 1567549]({{bugzilla}}1567549))
- Added support for exposing the content blocking log in [`ContentBlockingController`][71.7].
  ([bug 1580201]({{bugzilla}}1580201))
- ⚠️  Added `nativeApp` to [`WebExtension.MessageDelegate.onMessage`][71.8] which
  exposes the native application identifier that was used to send the message.
  ([bug 1546445]({{bugzilla}}1546445))
- Added [`GeckoRuntime.ServiceWorkerDelegate`][71.9] set via
  [`setServiceWorkerDelegate`][71.10] to support [`ServiceWorkerClients.openWindow`][71.11]
  ([bug 1511033]({{bugzilla}}1511033))
- Added [`GeckoRuntimeSettings.Builder#aboutConfigEnabled`][71.12] to control whether or
  not `about:config` should be available.
  ([bug 1540065]({{bugzilla}}1540065))
- Added [`GeckoSession.ContentDelegate.onFirstContentfulPaint`][71.13]
  ([bug 1578947]({{bugzilla}}1578947))
- Added `setEnhancedTrackingProtectionLevel` to [`ContentBlocking.Settings`][71.14].
  ([bug 1580854]({{bugzilla}}1580854))
- ⚠️ Added [`GeckoView.onTouchEventForResult`][71.15] and modified
  [`PanZoomController.onTouchEvent`][71.16] to return how the touch event was handled. This
  allows apps to know if an event is handled by touch event listeners in web content. The methods in `PanZoomController` now return `int` instead of `boolean`.
- Added [`GeckoSession.purgeHistory`][71.17] allowing apps to clear a session's history.
  ([bug 1583265]({{bugzilla}}1583265))
- Added [`GeckoRuntimeSettings.Builder#forceUserScalableEnabled`][71.18] to control whether or
  not to force user scalable zooming.
  ([bug 1540615]({{bugzilla}}1540615))
- ⚠️ Moved Autofill related methods from `SessionTextInput` and `GeckoSession.TextInputDelegate`
  into `GeckoSession` and `AutofillDelegate`.
- Added [`GeckoSession.getAutofillElements()`][71.19], which is a new method for getting
  an autofill virtual structure without using `ViewStructure`. It relies on a new class,
  [`AutofillElement`][71.20], for representing the virtual tree.
- Added [`GeckoView.setAutofillEnabled`][71.21] for controlling whether or not the `GeckoView`
  instance participates in Android autofill. When enabled, this connects an `AutofillDelegate`
  to the session it holds.
- Changed [`AutofillElement.children`][71.20] interface to `Collection` to provide
  an efficient way to pre-allocate memory when filling `ViewStructure`.
- Added [`GeckoSession.PromptDelegate.onSharePrompt`][71.22] to support the WebShare API.
  ([bug 1402369]({{bugzilla}}1402369))
- Added [`GeckoDisplay.screenshot`][71.23] allowing apps finer grain control over screenshots.
  ([bug 1577192]({{bugzilla}}1577192))
- Added `GeckoView.setDynamicToolbarMaxHeight` to make ICB size static, ICB doesn't include the dynamic toolbar region.
  ([bug 1586144]({{bugzilla}}1586144))

[71.1]: {{javadoc_uri}}/RuntimeTelemetry.Delegate.html#onBooleanScalar-org.mozilla.geckoview.RuntimeTelemetry.Metric-
[71.2]: {{javadoc_uri}}/RuntimeTelemetry.Delegate.html#onLongScalar-org.mozilla.geckoview.RuntimeTelemetry.Metric-
[71.3]: {{javadoc_uri}}/RuntimeTelemetry.Delegate.html#onStringScalar-org.mozilla.geckoview.RuntimeTelemetry.Metric-
[71.4]: {{javadoc_uri}}/RuntimeTelemetry.Delegate.html#onHistogram-org.mozilla.geckoview.RuntimeTelemetry.Metric-
[71.5]: {{javadoc_uri}}/RuntimeTelemetry.Metric.html
[71.6]: {{javadoc_uri}}/GeckoSession.html#loadUri-java.lang.String-java.io.File-java.util.Map-
[71.7]: {{javadoc_uri}}/ContentBlockingController.html
[71.8]: {{javadoc_uri}}/WebExtension.MessageDelegate.html#onMessage-java.lang.String-java.lang.Object-org.mozilla.geckoview.WebExtension.MessageSender-
[71.9]: {{javadoc_uri}}/GeckoRuntime.ServiceWorkerDelegate.html
[71.10]: {{javadoc_uri}}/GeckoRuntime#setServiceWorkerDelegate-org.mozilla.geckoview.GeckoRuntime.ServiceWorkerDelegate-
[71.11]: https://developer.mozilla.org/en-US/docs/Web/API/Clients/openWindow
[71.12]: {{javadoc_uri}}/GeckoRuntimeSettings.Builder.html#aboutConfigEnabled-boolean-
[71.13]: {{javadoc_uri}}/GeckoSession.ContentDelegate.html#onFirstContentfulPaint-org.mozilla.geckoview.GeckoSession-
[71.15]: {{javadoc_uri}}/GeckoView.html#onTouchEventForResult-android.view.MotionEvent-
[71.16]: {{javadoc_uri}}/PanZoomController.html#onTouchEvent-android.view.MotionEvent-
[71.17]: {{javadoc_uri}}/GeckoSession.html#purgeHistory--
[71.18]: {{javadoc_uri}}/GeckoRuntimeSettings.Builder.html#forceUserScalableEnabled-boolean-
[71.19]: {{javadoc_uri}}/GeckoSession.html#getAutofillElements--
[71.20]: {{javadoc_uri}}/AutofillElement.html
[71.21]: {{javadoc_uri}}/GeckoView.html#setAutofillEnabled-boolean-
[71.22]: {{javadoc_uri}}/GeckoSession.PromptDelegate.html#onSharePrompt-org.mozilla.geckoview.GeckoSession-org.mozilla.geckoview.GeckoSession.PromptDelegate.SharePrompt-
[71.23]: {{javadoc_uri}}/GeckoDisplay.html#screenshot--

## v70
- Added API for session context assignment
  [`GeckoSessionSettings.Builder.contextId`][70.1] and deletion of data related
  to a session context [`StorageController.clearDataForSessionContext`][70.2].
  ([bug 1501108]({{bugzilla}}1501108))
- Removed `setSession(session, runtime)` from [`GeckoView`][70.5]. With this
  change, `GeckoView` will no longer manage opening/closing of the
  [`GeckoSession`][70.6] and instead leave that up to the app. It's also now
  allowed to call [`setSession`][70.10] with a closed `GeckoSession`.
  ([bug 1510314]({{bugzilla}}1510314))
- Added an overload of [`GeckoSession.loadUri()`][70.8] that accepts a
  referring [`GeckoSession`][70.6]. This should be used when the URI we're
  loading originates from another page. A common example of this would be long
  pressing a link and then opening that in a new `GeckoSession`.
  ([bug 1561079]({{bugzilla}}1561079))
- Added capture parameter to [`onFilePrompt`][70.9] and corresponding
  [`CAPTURE_TYPE_*`][70.7] constants.
  ([bug 1553603]({{bugzilla}}1553603))
- Removed the obsolete `success` parameter from
  [`CrashReporter#sendCrashReport(Context, File, File, String)`][70.3] and
  [`CrashReporter#sendCrashReport(Context, File, Map, String)`][70.4].
  ([bug 1570789]({{bugzilla}}1570789))
- Add `GeckoSession.LOAD_FLAGS_REPLACE_HISTORY`.
  ([bug 1571088]({{bugzilla}}1571088))
- Complete rewrite of [`PromptDelegate`][70.11].
  ([bug 1499394]({{bugzilla}}1499394))
- Added [`RuntimeTelemetry.Delegate`][70.12] that receives streaming telemetry
  data from GeckoView.
  ([bug 1566367]({{bugzilla}}1566367))
- Updated [`ContentBlocking`][70.13] to better report blocked and allowed ETP events.
  ([bug 1567268]({{bugzilla}}1567268))
- Added API for controlling Gecko logging [`GeckoRuntimeSettings.debugLogging`][70.14]
  ([bug 1573304]({{bugzilla}}1573304))
- Added [`WebNotification`][70.15] and [`WebNotificationDelegate`][70.16] for handling Web Notifications.
  ([bug 1533057]({{bugzilla}}1533057))
- Added Social Tracking Protection support to [`ContentBlocking`][70.17].
  ([bug 1568295]({{bugzilla}}1568295))
- Added [`WebExtensionController`][70.18] and [`WebExtensionController.TabDelegate`][70.19] to handle
  [`browser.tabs.create`][70.20] calls by WebExtensions.
  ([bug 1539144]({{bugzilla}}1539144))
- Added [`onCloseTab`][70.21] to [`WebExtensionController.TabDelegate`][70.19] to handle
  [`browser.tabs.remove`][70.22] calls by WebExtensions.
  ([bug 1565782]({{bugzilla}}1565782))
- Added onSlowScript to [`ContentDelegate`][70.23] which allows handling of slow and hung scripts.
  ([bug 1621094]({{bugzilla}}1621094))
- Added support for Web Push via [`WebPushController`][70.24], [`WebPushDelegate`][70.25], and
  [`WebPushSubscription`][70.26].
- Added [`ContentBlockingController`][70.27], accessible via [`GeckoRuntime.getContentBlockingController`][70.28]
  to allow modification and inspection of a content blocking exception list.

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
[70.14]: {{javadoc_uri}}/GeckoRuntimeSettings.Builder.html#debugLogging-boolean-
[70.15]: {{javadoc_uri}}/WebNotification.html
[70.16]: {{javadoc_uri}}/WebNotificationDelegate.html
[70.17]: {{javadoc_uri}}/ContentBlocking.html
[70.18]: {{javadoc_uri}}/WebExtensionController.html
[70.19]: {{javadoc_uri}}/WebExtensionController.TabDelegate.html
[70.20]: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/tabs/create
[70.21]: {{javadoc_uri}}/WebExtensionController.TabDelegate.html#onCloseTab-org.mozilla.geckoview.WebExtension-org.mozilla.geckoview.GeckoSession-
[70.22]: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/tabs/remove
[70.23]: {{javadoc_uri}}/GeckoSession.ContentDelegate.html
[70.24]: {{javadoc_uri}}/WebPushController.html
[70.25]: {{javadoc_uri}}/WebPushDelegate.html
[70.26]: {{javadoc_uri}}/WebPushSubscription.html
[70.27]: {{javadoc_uri}}/ContentBlockingController.html
[70.28]: {{javadoc_uri}}/GeckoRuntime.html#getContentBlockingController--

## v69
- Modified behavior of ['setAutomaticFontSizeAdjustment'][69.1] so that it no
  longer has any effect on ['setFontInflationEnabled'][69.2]
- Add [GeckoSession.LOAD_FLAGS_FORCE_ALLOW_DATA_URI][69.14]
- Added [`GeckoResult.accept`][69.3] for consuming a result without
  transforming it.
- [`GeckoSession.setMessageDelegate`][69.13] callers must now specify the
  [`WebExtension`][69.5] that the [`MessageDelegate`][69.4] will receive
  messages from.
- Created [`onKill`][69.7] to [`ContentDelegate`][69.11] to differentiate from crashes.

[69.1]: {{javadoc_uri}}/GeckoRuntimeSettings.html#setAutomaticFontSizeAdjustment-boolean-
[69.2]: {{javadoc_uri}}/GeckoRuntimeSettings.html#setFontInflationEnabled-boolean-
[69.3]: {{javadoc_uri}}/GeckoResult.html#accept-org.mozilla.geckoview.GeckoResult.Consumer-
[69.4]: {{javadoc_uri}}/WebExtension.MessageDelegate.html
[69.5]: {{javadoc_uri}}/WebExtension.html
[69.7]: {{javadoc_uri}}/GeckoSession.ContentDelegate.html#onKill-org.mozilla.geckoview.GeckoSession-
[69.11]: {{javadoc_uri}}/GeckoSession.ContentDelegate.html
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

[api-version]: 266a352d20a71acc3052a2a9184e5a95a183145b
