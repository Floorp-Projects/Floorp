[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [SessionManager](./index.md)

# SessionManager

`class SessionManager : `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/SessionManager.kt#L28)

This class provides access to a centralized registry of all active sessions.

### Types

| Name | Summary |
|---|---|
| [EngineSessionLinker](-engine-session-linker/index.md) | `class EngineSessionLinker`<br>This class only exists for migrating from browser-session to browser-state. We need a way to dispatch the corresponding browser actions when an engine session is linked and unlinked. |
| [Observer](-observer/index.md) | `interface Observer`<br>Interface to be implemented by classes that want to observe the session manager. |
| [Snapshot](-snapshot/index.md) | `data class Snapshot` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SessionManager(engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`? = null, delegate: `[`LegacySessionManager`](../-legacy-session-manager/index.md)` = LegacySessionManager(engine, EngineSessionLinker(store)))`<br>This class provides access to a centralized registry of all active sessions. |

### Properties

| Name | Summary |
|---|---|
| [all](all.md) | `val all: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Session`](../-session/index.md)`>`<br>Returns a list of all active sessions (including CustomTab sessions). |
| [engine](engine.md) | `val engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md) |
| [selectedSession](selected-session.md) | `val selectedSession: `[`Session`](../-session/index.md)`?`<br>Gets the currently selected session if there is one. |
| [selectedSessionOrThrow](selected-session-or-throw.md) | `val selectedSessionOrThrow: `[`Session`](../-session/index.md)<br>Gets the currently selected session or throws an IllegalStateException if no session is selected. |
| [sessions](sessions.md) | `val sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Session`](../-session/index.md)`>`<br>Returns a list of active sessions and filters out sessions used for CustomTabs. |
| [size](size.md) | `val size: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Returns the number of session including CustomTab sessions. |

### Functions

| Name | Summary |
|---|---|
| [add](add.md) | `fun add(session: `[`Session`](../-session/index.md)`, selected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`? = null, parent: `[`Session`](../-session/index.md)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds the provided session.`fun add(sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Session`](../-session/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds multiple sessions. |
| [createSessionSnapshot](create-session-snapshot.md) | `fun createSessionSnapshot(session: `[`Session`](../-session/index.md)`): `[`Item`](-snapshot/-item/index.md)<br>Produces a [Snapshot.Item](-snapshot/-item/index.md) of a single [Session](../-session/index.md), suitable for restoring via [SessionManager.restore](restore.md). |
| [createSnapshot](create-snapshot.md) | `fun createSnapshot(): `[`Snapshot`](-snapshot/index.md)<br>Produces a snapshot of this manager's state, suitable for restoring via [SessionManager.restore](restore.md). Only regular sessions are included in the snapshot. Private and Custom Tab sessions are omitted. |
| [findSessionById](find-session-by-id.md) | `fun findSessionById(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Session`](../-session/index.md)`?`<br>Finds and returns the session with the given id. Returns null if no matching session could be found. |
| [getEngineSession](get-engine-session.md) | `fun getEngineSession(session: `[`Session`](../-session/index.md)` = selectedSessionOrThrow): `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?`<br>Gets the linked engine session for the provided session (if it exists). |
| [getOrCreateEngineSession](get-or-create-engine-session.md) | `fun getOrCreateEngineSession(session: `[`Session`](../-session/index.md)` = selectedSessionOrThrow): `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)<br>Gets the linked engine session for the provided session and creates it if needed. |
| [onLowMemory](on-low-memory.md) | `fun onLowMemory(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Informs this [SessionManager](./index.md) that the OS is in low memory condition so it can reduce its allocated objects. |
| [remove](remove.md) | `fun remove(session: `[`Session`](../-session/index.md)` = selectedSessionOrThrow, selectParentIfExists: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the provided session. If no session is provided then the selected session is removed. |
| [removeAll](remove-all.md) | `fun removeAll(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes all sessions including CustomTab sessions. |
| [removeSessions](remove-sessions.md) | `fun removeSessions(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes all sessions but CustomTab sessions. |
| [restore](restore.md) | `fun restore(snapshot: `[`Snapshot`](-snapshot/index.md)`, updateSelection: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Restores sessions from the provided [Snapshot](-snapshot/index.md). |
| [select](select.md) | `fun select(session: `[`Session`](../-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Marks the given session as selected. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [NO_SELECTION](-n-o_-s-e-l-e-c-t-i-o-n.md) | `const val NO_SELECTION: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [runWithSession](../run-with-session.md) | `fun `[`SessionManager`](./index.md)`.runWithSession(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, block: `[`SessionManager`](./index.md)`.(`[`Session`](../-session/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Tries to find a session with the provided session ID and runs the block if found. |
| [runWithSessionIdOrSelected](../run-with-session-id-or-selected.md) | `fun `[`SessionManager`](./index.md)`.runWithSessionIdOrSelected(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, block: `[`SessionManager`](./index.md)`.(`[`Session`](../-session/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Tries to find a session with the provided session ID or uses the selected session and runs the block if found. |
