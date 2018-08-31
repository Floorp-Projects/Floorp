---
title: ToolbarFeature - 
---

[mozilla.components.feature.toolbar](../index.html) / [ToolbarFeature](./index.html)

# ToolbarFeature

`class ToolbarFeature`

Feature implementation for connecting a toolbar implementation with the session module.

### Constructors

| [&lt;init&gt;](-init-.html) | `ToolbarFeature(toolbar: Toolbar, sessionManager: SessionManager, loadUrlUseCase: LoadUrlUseCase, searchUseCase: `[`SearchUseCase`](../-search-use-case.html)`? = null, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>Feature implementation for connecting a toolbar implementation with the session module. |

### Properties

| [toolbar](toolbar.html) | `val toolbar: Toolbar` |

### Functions

| [handleBackPressed](handle-back-pressed.html) | `fun handleBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Handler for back pressed events in activities that use this feature. |
| [start](start.html) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Start feature: App is in the foreground. |
| [stop](stop.html) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stop feature: App is in the background. |

