[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [LegacySessionManager](./index.md)

# LegacySessionManager

`class LegacySessionManager : `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](../-session-manager/-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/LegacySessionManager.kt#L18)

This class provides access to a centralized registry of all active sessions.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `LegacySessionManager(engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, delegate: `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](../-session-manager/-observer/index.md)`> = ObserverRegistry())`<br>This class provides access to a centralized registry of all active sessions. |

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
| [add](add.md) | `fun add(session: `[`Session`](../-session/index.md)`, selected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, parent: `[`Session`](../-session/index.md)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds the provided session.`fun add(sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Session`](../-session/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds multiple sessions. |
| [findSessionById](find-session-by-id.md) | `fun findSessionById(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Session`](../-session/index.md)`?`<br>Finds and returns the session with the given id. Returns null if no matching session could be found. |
| [remove](remove.md) | `fun remove(session: `[`Session`](../-session/index.md)` = selectedSessionOrThrow, selectParentIfExists: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the provided session. If no session is provided then the selected session is removed. |
| [removeAll](remove-all.md) | `fun removeAll(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes all sessions including CustomTab sessions. |
| [removeSessions](remove-sessions.md) | `fun removeSessions(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes all sessions but CustomTab sessions. |
| [restore](restore.md) | `fun restore(snapshot: `[`Snapshot`](../-session-manager/-snapshot/index.md)`, updateSelection: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Restores sessions from the provided [Snapshot](#). |
| [select](select.md) | `fun select(session: `[`Session`](../-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Marks the given session as selected. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [NO_SELECTION](-n-o_-s-e-l-e-c-t-i-o-n.md) | `const val NO_SELECTION: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
