---
title: TabsTrayPresenter - 
---

[mozilla.components.feature.tabs.tabstray](../index.html) / [TabsTrayPresenter](./index.html)

# TabsTrayPresenter

`class TabsTrayPresenter : Observer`

Presenter implementation for a tabs tray implementation in order to update the tabs tray whenever
the state of the session manager changes.

### Constructors

| [&lt;init&gt;](-init-.html) | `TabsTrayPresenter(tabsTray: TabsTray, sessionManager: SessionManager, closeTabsTray: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onTabsTrayEmpty: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null)`<br>Presenter implementation for a tabs tray implementation in order to update the tabs tray whenever the state of the session manager changes. |

### Functions

| [onAllSessionsRemoved](on-all-sessions-removed.html) | `fun onAllSessionsRemoved(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onSessionAdded](on-session-added.html) | `fun onSessionAdded(session: Session): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onSessionRemoved](on-session-removed.html) | `fun onSessionRemoved(session: Session): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onSessionSelected](on-session-selected.html) | `fun onSessionSelected(session: Session): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [start](start.html) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.html) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

