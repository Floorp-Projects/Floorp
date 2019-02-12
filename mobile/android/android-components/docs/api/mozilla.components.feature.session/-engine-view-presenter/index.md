[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [EngineViewPresenter](./index.md)

# EngineViewPresenter

`class EngineViewPresenter : `[`Observer`](../../mozilla.components.browser.session/-session-manager/-observer/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/EngineViewPresenter.kt#L14)

Presenter implementation for EngineView.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `EngineViewPresenter(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, engineView: `[`EngineView`](../../mozilla.components.concept.engine/-engine-view/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>Presenter implementation for EngineView. |

### Functions

| Name | Summary |
|---|---|
| [onSessionSelected](on-session-selected.md) | `fun onSessionSelected(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A new session has been selected: Render it on an EngineView. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Start presenter and display data in view. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stop presenter from updating view. |

### Inherited Functions

| Name | Summary |
|---|---|
| [onAllSessionsRemoved](../../mozilla.components.browser.session/-session-manager/-observer/on-all-sessions-removed.md) | `open fun onAllSessionsRemoved(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>All sessions have been removed. Note that this will callback will be invoked whenever removeAll() or removeSessions have been called on the SessionManager. This callback will NOT be invoked when just the last session has been removed by calling remove() on the SessionManager. |
| [onSessionAdded](../../mozilla.components.browser.session/-session-manager/-observer/on-session-added.md) | `open fun onSessionAdded(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The given session has been added. |
| [onSessionRemoved](../../mozilla.components.browser.session/-session-manager/-observer/on-session-removed.md) | `open fun onSessionRemoved(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The given session has been removed. |
| [onSessionsRestored](../../mozilla.components.browser.session/-session-manager/-observer/on-sessions-restored.md) | `open fun onSessionsRestored(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sessions have been restored via a snapshot. This callback is invoked at the end of the call to read, after every session in the snapshot was added, and appropriate session was selected. |
