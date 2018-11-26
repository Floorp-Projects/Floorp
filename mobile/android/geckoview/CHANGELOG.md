# v65
- Moved `CompositorController`, `DynamicToolbarAnimator`,
  `OverscrollEdgeEffect`, `PanZoomController` from `org.mozilla.gecko.gfx` to
  `org.mozilla.geckoview`
- Added `@UiThread`, `@AnyThread` annotations to several APIs
- Changed `GeckoRuntime#getLocale` to `GeckoRuntime#getLocales` and related APIs.
- Merged `org.mozilla.gecko.gfx.LayerSession` into `GeckoSession`
- Added `GeckoSession.MediaDelegate` and `MediaElement`. This allow monitoring
  and control of web media elements (play, pause, seek, etc).
- Removed unused `access` parameter from
  `GeckoSession.PermissionDelegate#onContentPermissionRequest`
- Added `WebMessage`, `WebRequest`, `WebResponse`, and `GeckoWebExecutor`. This
  exposes Gecko networking to apps. It includes speculative connections, name
  resolution, and a Fetch-like HTTP API.
- Added `GeckoSession.HistoryDelegate`. This allows apps to implement their own
  history storage system and provide visited link status.
- Added `ContentDelegate#onFirstComposite` to get first composite callback
  after a compositor start.
- Changed `LoadRequest.isUserTriggered` to `isRedirect`.
- Added `GeckoSession.LOAD_FLAGS_BYPASS_CLASSIFIER` to bypass the URI
  classifier.

[api-version]: 15d9fe901464f8b00016237cd8c0d954686bd07d
