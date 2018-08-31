---
title: ToolbarPresenter - 
---

[mozilla.components.feature.toolbar](../index.html) / [ToolbarPresenter](./index.html)

# ToolbarPresenter

`class ToolbarPresenter : Observer, Observer`

Presenter implementation for a toolbar implementation in order to update the toolbar whenever
the state of the selected session changes.

### Constructors

| [&lt;init&gt;](-init-.html) | `ToolbarPresenter(toolbar: Toolbar, sessionManager: SessionManager, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>Presenter implementation for a toolbar implementation in order to update the toolbar whenever the state of the selected session changes. |

### Functions

| [onProgress](on-progress.html) | `fun onProgress(session: Session, progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onSearch](on-search.html) | `fun onSearch(session: Session, searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onSessionSelected](on-session-selected.html) | `fun onSessionSelected(session: Session): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A new session has been selected: Update toolbar to display data of new session. |
| [onUrlChanged](on-url-changed.html) | `fun onUrlChanged(session: Session, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [start](start.html) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Start presenter: Display data in toolbar. |
| [stop](stop.html) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stop presenter from updating the view. |

