[android-components](../../index.md) / [mozilla.components.feature.tabs.tabstray](../index.md) / [TabsTrayPresenter](./index.md)

# TabsTrayPresenter

`class TabsTrayPresenter : `[`Observer`](../../mozilla.components.browser.session/-session-manager/-observer/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/tabstray/TabsTrayPresenter.kt#L17)

Presenter implementation for a tabs tray implementation in order to update the tabs tray whenever
the state of the session manager changes.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TabsTrayPresenter(tabsTray: `[`TabsTray`](../../mozilla.components.concept.tabstray/-tabs-tray/index.md)`, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, closeTabsTray: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, sessionsFilter: (`[`Session`](../../mozilla.components.browser.session/-session/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true })`<br>Presenter implementation for a tabs tray implementation in order to update the tabs tray whenever the state of the session manager changes. |

### Functions

| Name | Summary |
|---|---|
| [onAllSessionsRemoved](on-all-sessions-removed.md) | `fun onAllSessionsRemoved(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>All sessions have been removed. Note that this will callback will be invoked whenever removeAll() or removeSessions have been called on the SessionManager. This callback will NOT be invoked when just the last session has been removed by calling remove() on the SessionManager. |
| [onSessionAdded](on-session-added.md) | `fun onSessionAdded(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The given session has been added. |
| [onSessionRemoved](on-session-removed.md) | `fun onSessionRemoved(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The given session has been removed. |
| [onSessionSelected](on-session-selected.md) | `fun onSessionSelected(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The selection has changed and the given session is now the selected session. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inherited Functions

| Name | Summary |
|---|---|
| [onSessionsRestored](../../mozilla.components.browser.session/-session-manager/-observer/on-sessions-restored.md) | `open fun onSessionsRestored(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sessions have been restored via a snapshot. This callback is invoked at the end of the call to read, after every session in the snapshot was added, and appropriate session was selected. |
