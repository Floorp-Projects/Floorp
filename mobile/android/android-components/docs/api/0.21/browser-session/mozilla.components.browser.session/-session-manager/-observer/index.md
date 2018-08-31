---
title: SessionManager.Observer - 
---

[mozilla.components.browser.session](../../index.html) / [SessionManager](../index.html) / [Observer](./index.html)

# Observer

`interface Observer`

Interface to be implemented by classes that want to observe the session manager.

### Functions

| [onAllSessionsRemoved](on-all-sessions-removed.html) | `open fun onAllSessionsRemoved(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>All sessions have been removed. Note that this will callback will be invoked whenever removeAll() or removeSessions have been called on the SessionManager. This callback will NOT be invoked when just the last session has been removed by calling remove() on the SessionManager. |
| [onSessionAdded](on-session-added.html) | `open fun onSessionAdded(session: `[`Session`](../../-session/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The given session has been added. |
| [onSessionRemoved](on-session-removed.html) | `open fun onSessionRemoved(session: `[`Session`](../../-session/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The given session has been removed. |
| [onSessionSelected](on-session-selected.html) | `open fun onSessionSelected(session: `[`Session`](../../-session/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The selection has changed and the given session is now the selected session. |

