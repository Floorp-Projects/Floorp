---
title: EngineViewPresenter - 
---

[mozilla.components.feature.session](../index.html) / [EngineViewPresenter](./index.html)

# EngineViewPresenter

`class EngineViewPresenter : Observer`

Presenter implementation for EngineView.

### Constructors

| [&lt;init&gt;](-init-.html) | `EngineViewPresenter(sessionManager: SessionManager, engineView: EngineView, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>Presenter implementation for EngineView. |

### Functions

| [onSessionSelected](on-session-selected.html) | `fun onSessionSelected(session: Session): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A new session has been selected: Render it on an EngineView. |
| [start](start.html) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Start presenter and display data in view. |
| [stop](stop.html) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stop presenter from updating view. |

