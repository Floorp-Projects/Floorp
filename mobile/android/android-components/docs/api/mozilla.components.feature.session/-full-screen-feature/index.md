[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [FullScreenFeature](./index.md)

# FullScreenFeature

`open class FullScreenFeature : `[`SelectionAwareSessionObserver`](../../mozilla.components.browser.session/-selection-aware-session-observer/index.md)`, `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`BackHandler`](../../mozilla.components.support.base.feature/-back-handler/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/FullScreenFeature.kt#L16)

Feature implementation for handling fullscreen mode (exiting and back button presses).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FullScreenFeature(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionUseCases: `[`SessionUseCases`](../-session-use-cases/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, fullScreenChanged: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Feature implementation for handling fullscreen mode (exiting and back button presses). |

### Inherited Properties

| Name | Summary |
|---|---|
| [activeSession](../../mozilla.components.browser.session/-selection-aware-session-observer/active-session.md) | `open var activeSession: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`?`<br>the currently observed session |

### Functions

| Name | Summary |
|---|---|
| [onBackPressed](on-back-pressed.md) | `open fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>To be called when the back button is pressed, so that only fullscreen mode closes. |
| [onFullScreenChanged](on-full-screen-changed.md) | `open fun onFullScreenChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [start](start.md) | `open fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts the feature and a observer to listen for fullscreen changes. |

### Inherited Functions

| Name | Summary |
|---|---|
| [observeFixed](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-fixed.md) | `fun observeFixed(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the specified session. |
| [observeIdOrSelected](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-id-or-selected.md) | `fun observeIdOrSelected(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the session matching the [sessionId](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-id-or-selected.md#mozilla.components.browser.session.SelectionAwareSessionObserver$observeIdOrSelected(kotlin.String)/sessionId). If the session does not exist, then observe the selected session. |
| [observeSelected](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-selected.md) | `fun observeSelected(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the selected session (see [SessionManager.selectedSession](../../mozilla.components.browser.session/-session-manager/selected-session.md)). If a different session is selected the observer will automatically be switched over and only notified of changes to the newly selected session. |
| [onSessionSelected](../../mozilla.components.browser.session/-selection-aware-session-observer/on-session-selected.md) | `open fun onSessionSelected(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The selection has changed and the given session is now the selected session. |
| [stop](../../mozilla.components.browser.session/-selection-aware-session-observer/stop.md) | `open fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops the observer. |
