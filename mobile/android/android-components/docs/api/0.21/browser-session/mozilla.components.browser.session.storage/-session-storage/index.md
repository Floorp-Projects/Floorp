---
title: SessionStorage - 
---

[mozilla.components.browser.session.storage](../index.html) / [SessionStorage](./index.html)

# SessionStorage

`interface SessionStorage`

Storage component for browser and engine sessions.

### Functions

| [persist](persist.html) | `abstract fun persist(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Persists the state of the provided sessions. |
| [restore](restore.html) | `abstract fun restore(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Restores the session storage state by reading from the latest persisted version. |
| [start](start.html) | `abstract fun start(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts saving the state frequently and automatically. |
| [stop](stop.html) | `abstract fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops saving the state automatically. |

### Inheritors

| [DefaultSessionStorage](../-default-session-storage/index.html) | `class DefaultSessionStorage : `[`SessionStorage`](./index.md)<br>Default implementation of [SessionStorage](./index.md) which persists browser and engine session states to a JSON file. |

