---
title: ToolbarInteractor - 
---

[mozilla.components.feature.toolbar](../index.html) / [ToolbarInteractor](./index.html)

# ToolbarInteractor

`class ToolbarInteractor`

Connects a toolbar instance to the browser engine via use cases

### Constructors

| [&lt;init&gt;](-init-.html) | `ToolbarInteractor(toolbar: Toolbar, loadUrlUseCase: LoadUrlUseCase, searchUseCase: `[`SearchUseCase`](../-search-use-case.html)`? = null)`<br>Connects a toolbar instance to the browser engine via use cases |

### Functions

| [start](start.html) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts this interactor. Makes sure this interactor is listening to relevant UI changes and triggers the corresponding use-cases in response. |
| [stop](stop.html) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops this interactor. |

