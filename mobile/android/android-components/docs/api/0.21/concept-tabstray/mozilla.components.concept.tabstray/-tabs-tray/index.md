---
title: TabsTray - 
---

[mozilla.components.concept.tabstray](../index.html) / [TabsTray](./index.html)

# TabsTray

`interface TabsTray : Observable<`[`Observer`](-observer/index.html)`>`

Generic interface for components that provide "tabs tray" functionality.

### Types

| [Observer](-observer/index.html) | `interface Observer`<br>Interface to be implemented by classes that want to observe a tabs tray. |

### Functions

| [asView](as-view.html) | `open fun asView(): View`<br>Convenience method to cast the implementation of this interface to an Android View object. |
| [displaySessions](display-sessions.html) | `abstract fun displaySessions(sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<Session>, selectedIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays the given list of sessions. |
| [onSessionMoved](on-session-moved.html) | `abstract fun onSessionMoved(fromPosition: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, toPosition: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called after updateSessions() when a session changes it position. |
| [onSessionsChanged](on-sessions-changed.html) | `abstract fun onSessionsChanged(position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, count: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called after updateSessions() when count number of sessions are updated at the given position. |
| [onSessionsInserted](on-sessions-inserted.html) | `abstract fun onSessionsInserted(position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, count: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called after updateSessions() when count number of sessions are inserted at the given position. |
| [onSessionsRemoved](on-sessions-removed.html) | `abstract fun onSessionsRemoved(position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, count: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called after updateSessions() when count number of sessions are removed from the given position. |
| [updateSessions](update-sessions.html) | `abstract fun updateSessions(sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<Session>, selectedIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Updates the list of sessions. |

