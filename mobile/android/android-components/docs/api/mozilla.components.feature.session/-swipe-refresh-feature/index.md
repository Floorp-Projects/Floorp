[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [SwipeRefreshFeature](./index.md)

# SwipeRefreshFeature

`class SwipeRefreshFeature : `[`SelectionAwareSessionObserver`](../../mozilla.components.browser.session/-selection-aware-session-observer/index.md)`, `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, OnChildScrollUpCallback, OnRefreshListener` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SwipeRefreshFeature.kt#L20)

Feature implementation to add pull to refresh functionality to browsers.

### Parameters

`swipeRefreshLayout` - Reference to SwipeRefreshLayout that has an [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) as its child.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SwipeRefreshFeature(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, reloadUrlUseCase: `[`ReloadUrlUseCase`](../-session-use-cases/-reload-url-use-case/index.md)`, swipeRefreshLayout: SwipeRefreshLayout, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>Feature implementation to add pull to refresh functionality to browsers. |

### Inherited Properties

| Name | Summary |
|---|---|
| [activeSession](../../mozilla.components.browser.session/-selection-aware-session-observer/active-session.md) | `open var activeSession: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`?`<br>the currently observed session |

### Functions

| Name | Summary |
|---|---|
| [canChildScrollUp](can-child-scroll-up.md) | `fun canChildScrollUp(parent: SwipeRefreshLayout, child: <ERROR CLASS>?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Callback that checks whether it is possible for the child view to scroll up. If the child view cannot scroll up, attempted to scroll up triggers a refresh gesture. |
| [onLoadingStateChanged](on-loading-state-changed.md) | `fun onLoadingStateChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, loading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called when the current session starts or stops refreshing. |
| [onRefresh](on-refresh.md) | `fun onRefresh(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called when a swipe gesture triggers a refresh. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Start feature: Starts adding pull to refresh behavior for the active session. |

### Inherited Functions

| Name | Summary |
|---|---|
| [observeFixed](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-fixed.md) | `fun observeFixed(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the specified session. |
| [observeIdOrSelected](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-id-or-selected.md) | `fun observeIdOrSelected(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the session matching the [sessionId](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-id-or-selected.md#mozilla.components.browser.session.SelectionAwareSessionObserver$observeIdOrSelected(kotlin.String)/sessionId). If the session does not exist, then observe the selected session. |
| [observeSelected](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-selected.md) | `fun observeSelected(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the selected session (see [SessionManager.selectedSession](../../mozilla.components.browser.session/-session-manager/selected-session.md)). If a different session is selected the observer will automatically be switched over and only notified of changes to the newly selected session. |
| [onSessionSelected](../../mozilla.components.browser.session/-selection-aware-session-observer/on-session-selected.md) | `open fun onSessionSelected(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The selection has changed and the given session is now the selected session. |
| [stop](../../mozilla.components.browser.session/-selection-aware-session-observer/stop.md) | `open fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops the observer. |
