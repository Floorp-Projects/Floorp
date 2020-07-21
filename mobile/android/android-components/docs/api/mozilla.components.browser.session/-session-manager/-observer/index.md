[android-components](../../../index.md) / [mozilla.components.browser.session](../../index.md) / [SessionManager](../index.md) / [Observer](./index.md)

# Observer

`interface Observer` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/SessionManager.kt#L475)

Interface to be implemented by classes that want to observe the session manager.

### Functions

| Name | Summary |
|---|---|
| [onAllSessionsRemoved](on-all-sessions-removed.md) | `open fun onAllSessionsRemoved(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>All sessions have been removed. Note that this will callback will be invoked whenever removeAll() or removeSessions have been called on the SessionManager. This callback will NOT be invoked when just the last session has been removed by calling remove() on the SessionManager. |
| [onSessionAdded](on-session-added.md) | `open fun onSessionAdded(session: `[`Session`](../../-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The given session has been added. |
| [onSessionRemoved](on-session-removed.md) | `open fun onSessionRemoved(session: `[`Session`](../../-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The given session has been removed. |
| [onSessionSelected](on-session-selected.md) | `open fun onSessionSelected(session: `[`Session`](../../-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The selection has changed and the given session is now the selected session. |
| [onSessionsRestored](on-sessions-restored.md) | `open fun onSessionsRestored(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sessions have been restored via a snapshot. This callback is invoked at the end of the call to read, after every session in the snapshot was added, and appropriate session was selected. |

### Inheritors

| Name | Summary |
|---|---|
| [SelectionAwareSessionObserver](../../-selection-aware-session-observer/index.md) | `abstract class SelectionAwareSessionObserver : `[`Observer`](./index.md)`, `[`Observer`](../../-session/-observer/index.md)<br>This class is a combination of [Session.Observer](../../-session/-observer/index.md) and [SessionManager.Observer](./index.md). It provides functionality to observe changes to a specified or selected session, and can automatically take care of switching over the observer in case a different session gets selected (see [observeFixed](../../-selection-aware-session-observer/observe-fixed.md) and [observeSelected](../../-selection-aware-session-observer/observe-selected.md)). |
