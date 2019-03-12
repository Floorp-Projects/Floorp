[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [SelectionAwareSessionObserver](./index.md)

# SelectionAwareSessionObserver

`abstract class SelectionAwareSessionObserver : `[`Observer`](../-session-manager/-observer/index.md)`, `[`Observer`](../-session/-observer/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/SelectionAwareSessionObserver.kt#L19)

This class is a combination of [Session.Observer](../-session/-observer/index.md) and
[SessionManager.Observer](../-session-manager/-observer/index.md). It provides functionality to observe changes to a
specified or selected session, and can automatically take care of switching
over the observer in case a different session gets selected (see
[observeFixed](observe-fixed.md) and [observeSelected](observe-selected.md)).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SelectionAwareSessionObserver(sessionManager: `[`SessionManager`](../-session-manager/index.md)`)`<br>This class is a combination of [Session.Observer](../-session/-observer/index.md) and [SessionManager.Observer](../-session-manager/-observer/index.md). It provides functionality to observe changes to a specified or selected session, and can automatically take care of switching over the observer in case a different session gets selected (see [observeFixed](observe-fixed.md) and [observeSelected](observe-selected.md)). |

### Properties

| Name | Summary |
|---|---|
| [activeSession](active-session.md) | `open var activeSession: `[`Session`](../-session/index.md)`?`<br>the currently observed session |

### Functions

| Name | Summary |
|---|---|
| [observeFixed](observe-fixed.md) | `fun observeFixed(session: `[`Session`](../-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the specified session. |
| [observeIdOrSelected](observe-id-or-selected.md) | `fun observeIdOrSelected(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the session matching the [sessionId](observe-id-or-selected.md#mozilla.components.browser.session.SelectionAwareSessionObserver$observeIdOrSelected(kotlin.String)/sessionId). If the session does not exist, then observe the selected session. |
| [observeSelected](observe-selected.md) | `fun observeSelected(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the selected session (see [SessionManager.selectedSession](../-session-manager/selected-session.md)). If a different session is selected the observer will automatically be switched over and only notified of changes to the newly selected session. |
| [onSessionSelected](on-session-selected.md) | `open fun onSessionSelected(session: `[`Session`](../-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The selection has changed and the given session is now the selected session. |
| [stop](stop.md) | `open fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops the observer. |

### Inherited Functions

| Name | Summary |
|---|---|
| [onAllSessionsRemoved](../-session-manager/-observer/on-all-sessions-removed.md) | `open fun onAllSessionsRemoved(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>All sessions have been removed. Note that this will callback will be invoked whenever removeAll() or removeSessions have been called on the SessionManager. This callback will NOT be invoked when just the last session has been removed by calling remove() on the SessionManager. |
| [onAppPermissionRequested](../-session/-observer/on-app-permission-requested.md) | `open fun onAppPermissionRequested(session: `[`Session`](../-session/index.md)`, permissionRequest: `[`PermissionRequest`](../../mozilla.components.concept.engine.permission/-permission-request/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onCloseWindowRequested](../-session/-observer/on-close-window-requested.md) | `open fun onCloseWindowRequested(session: `[`Session`](../-session/index.md)`, windowRequest: `[`WindowRequest`](../../mozilla.components.concept.engine.window/-window-request/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onContentPermissionRequested](../-session/-observer/on-content-permission-requested.md) | `open fun onContentPermissionRequested(session: `[`Session`](../-session/index.md)`, permissionRequest: `[`PermissionRequest`](../../mozilla.components.concept.engine.permission/-permission-request/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onCustomTabConfigChanged](../-session/-observer/on-custom-tab-config-changed.md) | `open fun onCustomTabConfigChanged(session: `[`Session`](../-session/index.md)`, customTabConfig: `[`CustomTabConfig`](../../mozilla.components.browser.session.tab/-custom-tab-config/index.md)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onDesktopModeChanged](../-session/-observer/on-desktop-mode-changed.md) | `open fun onDesktopModeChanged(session: `[`Session`](../-session/index.md)`, enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onDownload](../-session/-observer/on-download.md) | `open fun onDownload(session: `[`Session`](../-session/index.md)`, download: `[`Download`](../-download/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onFindResult](../-session/-observer/on-find-result.md) | `open fun onFindResult(session: `[`Session`](../-session/index.md)`, result: `[`FindResult`](../-session/-find-result/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onFullScreenChanged](../-session/-observer/on-full-screen-changed.md) | `open fun onFullScreenChanged(session: `[`Session`](../-session/index.md)`, enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onLoadingStateChanged](../-session/-observer/on-loading-state-changed.md) | `open fun onLoadingStateChanged(session: `[`Session`](../-session/index.md)`, loading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onLongPress](../-session/-observer/on-long-press.md) | `open fun onLongPress(session: `[`Session`](../-session/index.md)`, hitResult: `[`HitResult`](../../mozilla.components.concept.engine/-hit-result/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onNavigationStateChanged](../-session/-observer/on-navigation-state-changed.md) | `open fun onNavigationStateChanged(session: `[`Session`](../-session/index.md)`, canGoBack: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, canGoForward: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onOpenWindowRequested](../-session/-observer/on-open-window-requested.md) | `open fun onOpenWindowRequested(session: `[`Session`](../-session/index.md)`, windowRequest: `[`WindowRequest`](../../mozilla.components.concept.engine.window/-window-request/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onProgress](../-session/-observer/on-progress.md) | `open fun onProgress(session: `[`Session`](../-session/index.md)`, progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onPromptRequested](../-session/-observer/on-prompt-requested.md) | `open fun onPromptRequested(session: `[`Session`](../-session/index.md)`, promptRequest: `[`PromptRequest`](../../mozilla.components.concept.engine.prompt/-prompt-request/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onSearch](../-session/-observer/on-search.md) | `open fun onSearch(session: `[`Session`](../-session/index.md)`, searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onSecurityChanged](../-session/-observer/on-security-changed.md) | `open fun onSecurityChanged(session: `[`Session`](../-session/index.md)`, securityInfo: `[`SecurityInfo`](../-session/-security-info/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onSessionAdded](../-session-manager/-observer/on-session-added.md) | `open fun onSessionAdded(session: `[`Session`](../-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The given session has been added. |
| [onSessionRemoved](../-session-manager/-observer/on-session-removed.md) | `open fun onSessionRemoved(session: `[`Session`](../-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The given session has been removed. |
| [onSessionsRestored](../-session-manager/-observer/on-sessions-restored.md) | `open fun onSessionsRestored(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sessions have been restored via a snapshot. This callback is invoked at the end of the call to read, after every session in the snapshot was added, and appropriate session was selected. |
| [onThumbnailChanged](../-session/-observer/on-thumbnail-changed.md) | `open fun onThumbnailChanged(session: `[`Session`](../-session/index.md)`, bitmap: `[`Bitmap`](https://developer.android.com/reference/android/graphics/Bitmap.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onTitleChanged](../-session/-observer/on-title-changed.md) | `open fun onTitleChanged(session: `[`Session`](../-session/index.md)`, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onTrackerBlocked](../-session/-observer/on-tracker-blocked.md) | `open fun onTrackerBlocked(session: `[`Session`](../-session/index.md)`, blocked: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, all: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onTrackerBlockingEnabledChanged](../-session/-observer/on-tracker-blocking-enabled-changed.md) | `open fun onTrackerBlockingEnabledChanged(session: `[`Session`](../-session/index.md)`, blockingEnabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onUrlChanged](../-session/-observer/on-url-changed.md) | `open fun onUrlChanged(session: `[`Session`](../-session/index.md)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onWebAppManifestChanged](../-session/-observer/on-web-app-manifest-changed.md) | `open fun onWebAppManifestChanged(session: `[`Session`](../-session/index.md)`, manifest: `[`WebAppManifest`](../../mozilla.components.browser.session.manifest/-web-app-manifest/index.md)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [CoordinateScrollingFeature](../../mozilla.components.feature.session/-coordinate-scrolling-feature/index.md) | `class CoordinateScrollingFeature : `[`SelectionAwareSessionObserver`](./index.md)`, `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)<br>Feature implementation for connecting an [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) with any View that you want to coordinate scrolling behavior with. |
| [DownloadsFeature](../../mozilla.components.feature.downloads/-downloads-feature/index.md) | `class DownloadsFeature : `[`SelectionAwareSessionObserver`](./index.md)`, `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)<br>Feature implementation to provide download functionality for the selected session. The feature will subscribe to the selected session and listen for downloads. |
| [FullScreenFeature](../../mozilla.components.feature.session/-full-screen-feature/index.md) | `open class FullScreenFeature : `[`SelectionAwareSessionObserver`](./index.md)`, `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`BackHandler`](../../mozilla.components.support.base.feature/-back-handler/index.md)<br>Feature implementation for handling fullscreen mode (exiting and back button presses). |
| [ToolbarPresenter](../../mozilla.components.feature.toolbar/-toolbar-presenter/index.md) | `class ToolbarPresenter : `[`SelectionAwareSessionObserver`](./index.md)<br>Presenter implementation for a toolbar implementation in order to update the toolbar whenever the state of the selected session changes. |
