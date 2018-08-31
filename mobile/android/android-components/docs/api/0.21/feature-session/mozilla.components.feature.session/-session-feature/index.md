---
title: SessionFeature - 
---

[mozilla.components.feature.session](../index.html) / [SessionFeature](./index.html)

# SessionFeature

`class SessionFeature`

Feature implementation for connecting the engine module with the session module.

### Constructors

| [&lt;init&gt;](-init-.html) | `SessionFeature(sessionManager: SessionManager, sessionUseCases: `[`SessionUseCases`](../-session-use-cases/index.html)`, engineView: EngineView, sessionStorage: SessionStorage? = null, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>Feature implementation for connecting the engine module with the session module. |

### Functions

| [handleBackPressed](handle-back-pressed.html) | `fun handleBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Handler for back pressed events in activities that use this feature. |
| [start](start.html) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Start feature: App is in the foreground. |
| [stop](stop.html) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stop feature: App is in the background. |

