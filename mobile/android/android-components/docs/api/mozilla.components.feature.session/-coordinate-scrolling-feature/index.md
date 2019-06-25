[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [CoordinateScrollingFeature](./index.md)

# CoordinateScrollingFeature

`class CoordinateScrollingFeature : `[`SelectionAwareSessionObserver`](../../mozilla.components.browser.session/-selection-aware-session-observer/index.md)`, `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/CoordinateScrollingFeature.kt#L25)

Feature implementation for connecting an [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) with any View that you want to coordinate scrolling
behavior with.

A use case could be collapsing a toolbar every time that the user scrolls.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CoordinateScrollingFeature(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, engineView: `[`EngineView`](../../mozilla.components.concept.engine/-engine-view/index.md)`, view: <ERROR CLASS>, scrollFlags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = DEFAULT_SCROLL_FLAGS)`<br>Feature implementation for connecting an [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) with any View that you want to coordinate scrolling behavior with. |

### Inherited Properties

| Name | Summary |
|---|---|
| [activeSession](../../mozilla.components.browser.session/-selection-aware-session-observer/active-session.md) | `open var activeSession: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`?`<br>the currently observed session |

### Functions

| Name | Summary |
|---|---|
| [onLoadingStateChanged](on-loading-state-changed.md) | `fun onLoadingStateChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, loading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Start feature: Starts adding scrolling behavior for the indicated view. |

### Inherited Functions

| Name | Summary |
|---|---|
| [observeFixed](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-fixed.md) | `fun observeFixed(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the specified session. |
| [observeIdOrSelected](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-id-or-selected.md) | `fun observeIdOrSelected(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the session matching the [sessionId](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-id-or-selected.md#mozilla.components.browser.session.SelectionAwareSessionObserver$observeIdOrSelected(kotlin.String)/sessionId). If the session does not exist, then observe the selected session. |
| [observeSelected](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-selected.md) | `fun observeSelected(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the selected session (see [SessionManager.selectedSession](../../mozilla.components.browser.session/-session-manager/selected-session.md)). If a different session is selected the observer will automatically be switched over and only notified of changes to the newly selected session. |
| [onSessionSelected](../../mozilla.components.browser.session/-selection-aware-session-observer/on-session-selected.md) | `open fun onSessionSelected(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The selection has changed and the given session is now the selected session. |
| [stop](../../mozilla.components.browser.session/-selection-aware-session-observer/stop.md) | `open fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops the observer. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [DEFAULT_SCROLL_FLAGS](-d-e-f-a-u-l-t_-s-c-r-o-l-l_-f-l-a-g-s.md) | `const val DEFAULT_SCROLL_FLAGS: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
