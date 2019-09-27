[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [LegacySessionManager](./index.md)

# LegacySessionManager

`class LegacySessionManager : `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](../-session-manager/-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/LegacySessionManager.kt#L20)

This class provides access to a centralized registry of all active sessions.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `LegacySessionManager(engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, engineSessionLinker: `[`EngineSessionLinker`](../-session-manager/-engine-session-linker/index.md)`, delegate: `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](../-session-manager/-observer/index.md)`> = ObserverRegistry())`<br>This class provides access to a centralized registry of all active sessions. |

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
| [createSessionSnapshot](create-session-snapshot.md) | `fun createSessionSnapshot(session: `[`Session`](../-session/index.md)`): `[`Item`](../-session-manager/-snapshot/-item/index.md) |
| [createSnapshot](create-snapshot.md) | `fun createSnapshot(): `[`Snapshot`](../-session-manager/-snapshot/index.md)<br>Produces a snapshot of this manager's state, suitable for restoring via [SessionManager.restore](../-session-manager/restore.md). Only regular sessions are included in the snapshot. Private and Custom Tab sessions are omitted. |
| [findSessionById](find-session-by-id.md) | `fun findSessionById(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Session`](../-session/index.md)`?`<br>Finds and returns the session with the given id. Returns null if no matching session could be found. |
| [getEngineSession](get-engine-session.md) | `fun getEngineSession(session: `[`Session`](../-session/index.md)` = selectedSessionOrThrow): `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?`<br>Gets the linked engine session for the provided session (if it exists). |
| [getOrCreateEngineSession](get-or-create-engine-session.md) | `fun getOrCreateEngineSession(session: `[`Session`](../-session/index.md)` = selectedSessionOrThrow): `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)<br>Gets the linked engine session for the provided session and creates it if needed. |
| [link](link.md) | `fun link(session: `[`Session`](../-session/index.md)`, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onLowMemory](on-low-memory.md) | `fun onLowMemory(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Informs this [SessionManager](../-session-manager/index.md) that the OS is in low memory condition so it can reduce its allocated objects. |
| [remove](remove.md) | `fun remove(session: `[`Session`](../-session/index.md)` = selectedSessionOrThrow, selectParentIfExists: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the provided session. If no session is provided then the selected session is removed. |
| [removeAll](remove-all.md) | `fun removeAll(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes all sessions including CustomTab sessions. |
| [removeSessions](remove-sessions.md) | `fun removeSessions(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes all sessions but CustomTab sessions. |
| [restore](restore.md) | `fun restore(snapshot: `[`Snapshot`](../-session-manager/-snapshot/index.md)`, updateSelection: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Restores sessions from the provided [Snapshot](#). |
| [select](select.md) | `fun select(session: `[`Session`](../-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Marks the given session as selected. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [NO_SELECTION](-n-o_-s-e-l-e-c-t-i-o-n.md) | `const val NO_SELECTION: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
