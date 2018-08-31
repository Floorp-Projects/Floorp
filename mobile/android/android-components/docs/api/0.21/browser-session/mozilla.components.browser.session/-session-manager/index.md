---
title: SessionManager - 
---

[mozilla.components.browser.session](../index.html) / [SessionManager](./index.html)

# SessionManager

`class SessionManager : Observable<`[`Observer`](-observer/index.html)`>`

This class provides access to a centralized registry of all active sessions.

### Types

| [Observer](-observer/index.html) | `interface Observer`<br>Interface to be implemented by classes that want to observe the session manager. |

### Constructors

| [&lt;init&gt;](-init-.html) | `SessionManager(engine: Engine, delegate: Observable<`[`Observer`](-observer/index.html)`> = ObserverRegistry())`<br>This class provides access to a centralized registry of all active sessions. |

### Properties

| [all](all.html) | `val all: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Session`](../-session/index.html)`>`<br>Returns a list of all active sessions (including CustomTab sessions). |
| [engine](engine.html) | `val engine: Engine` |
| [selectedSession](selected-session.html) | `val selectedSession: `[`Session`](../-session/index.html)`?`<br>Gets the currently selected session if there is one. |
| [selectedSessionOrThrow](selected-session-or-throw.html) | `val selectedSessionOrThrow: `[`Session`](../-session/index.html)<br>Gets the currently selected session or throws an IllegalStateException if no session is selected. |
| [sessions](sessions.html) | `val sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Session`](../-session/index.html)`>`<br>Returns a list of active sessions and filters out sessions used for CustomTabs. |
| [size](size.html) | `val size: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Returns the number of session including CustomTab sessions. |

### Functions

| [add](add.html) | `fun add(session: `[`Session`](../-session/index.html)`, selected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, engineSession: EngineSession? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds the provided session. |
| [findSessionById](find-session-by-id.html) | `fun findSessionById(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Session`](../-session/index.html)`?`<br>Finds and returns the session with the given id. Returns null if no matching session could be found. |
| [getEngineSession](get-engine-session.html) | `fun getEngineSession(session: `[`Session`](../-session/index.html)` = selectedSessionOrThrow): EngineSession?`<br>Gets the linked engine session for the provided session (if it exists). |
| [getOrCreateEngineSession](get-or-create-engine-session.html) | `fun getOrCreateEngineSession(session: `[`Session`](../-session/index.html)` = selectedSessionOrThrow): EngineSession`<br>Gets the linked engine session for the provided session and creates it if needed. |
| [remove](remove.html) | `fun remove(session: `[`Session`](../-session/index.html)` = selectedSessionOrThrow): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the provided session. If no session is provided then the selected session is removed. |
| [removeAll](remove-all.html) | `fun removeAll(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes all sessions including CustomTab sessions. |
| [removeSessions](remove-sessions.html) | `fun removeSessions(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes all sessions but CustomTab sessions. |
| [select](select.html) | `fun select(session: `[`Session`](../-session/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Marks the given session as selected. |

### Companion Object Properties

| [NO_SELECTION](-n-o_-s-e-l-e-c-t-i-o-n.html) | `const val NO_SELECTION: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

