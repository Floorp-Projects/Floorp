---
layout: geckoview
---

<h1> GeckoView API Changelog. </h1>

## v66
- Added [`@NonNull`][66.1] or [`@Nullable`][66.2] to all APIs.

[66.1]: https://developer.android.com/reference/android/support/annotation/NonNull
[66.2]: https://developer.android.com/reference/android/support/annotation/Nullable

- Added methods for each setting in [`GeckoSessionSettings`][66.3]

[66.3]: ../GeckoSessionSettings.html

- Added GeckoRuntimeSetting for enabling desktop viewport. Desktop viewport is
  no longer set by `USER_AGENT_MODE_DESKTOP` and must be set separately.

## v65
- Moved [`CompositorController`][65.1], [`DynamicToolbarAnimator`][65.2],
  [`OverscrollEdgeEffect`][65.3], [`PanZoomController`][65.4] from
  `org.mozilla.gecko.gfx` to [`org.mozilla.geckoview`][65.5]

[65.1]: ../CompositorController.html
[65.2]: ../DynamicToolbarAnimator.html
[65.3]: ../OverscrollEdgeEffect.html
[65.4]: ../PanZoomController.html
[65.5]: ../package-summary.html

- Added [`@UiThread`][65.6], [`@AnyThread`][65.7] annotations to all APIs

[65.6]: https://developer.android.com/reference/android/support/annotation/UiThread
[65.7]: https://developer.android.com/reference/android/support/annotation/AnyThread

- Changed `GeckoRuntimeSettings#getLocale` to [`getLocales`][65.8] and related
  APIs.

[65.8]: ../GeckoRuntimeSettings.html#getLocales--

- Merged `org.mozilla.gecko.gfx.LayerSession` into [`GeckoSession`][65.9]

[65.9]: ../GeckoSession.html

- Added [`GeckoSession.MediaDelegate`][65.10] and [`MediaElement`][65.11]. This
  allow monitoring and control of web media elements (play, pause, seek, etc).

[65.10]: ../GeckoSession.MediaDelegate.html
[65.11]: ../MediaElement.html

- Removed unused `access` parameter from
  [`GeckoSession.PermissionDelegate#onContentPermissionRequest`][65.12]

[65.12]: ../GeckoSession.PermissionDelegate.html#onContentPermissionRequest-org.mozilla.geckoview.GeckoSession-java.lang.String-int-org.mozilla.geckoview.GeckoSession.PermissionDelegate.Callback-

- Added [`WebMessage`][65.13], [`WebRequest`][65.14], [`WebResponse`][65.15],
  and [`GeckoWebExecutor`][65.16]. This exposes Gecko networking to apps. It
  includes speculative connections, name resolution, and a Fetch-like HTTP API.

[65.13]: ../WebMessage.html
[65.14]: ../WebRequest.html
[65.15]: ../WebResponse.html
[65.16]: ../GeckoWebExecutor.html

- Added [`GeckoSession.HistoryDelegate`][65.17]. This allows apps to implement
  their own history storage system and provide visited link status.

[65.17]: ../GeckoSession.HistoryDelegate.html

- Added [`ContentDelegate#onFirstComposite`][65.18] to get first composite
  callback after a compositor start.

[65.18]: ../GeckoSession.ContentDelegate.html#onFirstComposite-org.mozilla.geckoview.GeckoSession-

- Changed `LoadRequest.isUserTriggered` to [`isRedirect`][65.19].

[65.19]: ../GeckoSession.NavigationDelegate.LoadRequest.html#isRedirect

- Added [`GeckoSession.LOAD_FLAGS_BYPASS_CLASSIFIER`][65.20] to bypass the URI
  classifier.

[65.20]: ../GeckoSession.html#LOAD_FLAGS_BYPASS_CLASSIFIER

- Added a `protected` empty constructor to all field-only classes so that apps
  can mock these classes in tests.

- Added [`ContentDelegate.ContextElement`][65.21] to extend the information
  passed to [`ContentDelegate#onContextMenu`][65.22]. Extended information
  includes the element's title and alt attributes.

[65.21]: ../GeckoSession.ContentDelegate.ContextElement.html
[65.22]: ../GeckoSession.ContentDelegate.html#onContextMenu-org.mozilla.geckoview.GeckoSession-int-int-org.mozilla.geckoview.GeckoSession.ContentDelegate.ContextElement-

- Changed [`ContentDelegate.ContextElement`][65.21] `TYPE_` constants to public
  access.

- Changed [`ContentDelegate.ContextElement`][65.21],
  [`GeckoSession.FinderResult`][65.23] to non-final class.

[65.23]: ../GeckoSession.FinderResult.html

- Update [`CrashReporter#sendCrashReport`][65.24] to return the crash ID as a
  [`GeckoResult<String>`][65.25].

[65.24]: ../CrashReporter.html#sendCrashReport-android.content.Context-android.os.Bundle-java.lang.String-
[65.25]: ../GeckoResult.html

[api-version]: 5957a5943b39ae0e56b7e892bd824a16bb71e811
