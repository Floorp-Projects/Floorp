---
title: DefaultSessionStorage - 
---

[mozilla.components.browser.session.storage](../index.html) / [DefaultSessionStorage](./index.html)

# DefaultSessionStorage

`class DefaultSessionStorage : `[`SessionStorage`](../-session-storage/index.html)

Default implementation of [SessionStorage](../-session-storage/index.html) which persists browser and engine
session states to a JSON file.

The JSON format used for persisting:
{
    "version": [version](#),
    "selectedSession": "[session-uuid](#)",
    "[session-uuid](#)": {
        "session": {}
        "engineSession": {}
    },
    "[session-uuid](#)": {
        "session": {}
        "engineSession": {}
    }
}

### Constructors

| [&lt;init&gt;](-init-.html) | `DefaultSessionStorage(context: Context, savePeriodically: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, saveIntervalInSeconds: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 300, scheduler: `[`ScheduledExecutorService`](http://docs.oracle.com/javase/6/docs/api/java/util/concurrent/ScheduledExecutorService.html)` = Executors.newSingleThreadScheduledExecutor())`<br>Default implementation of [SessionStorage](../-session-storage/index.html) which persists browser and engine session states to a JSON file. |

### Functions

| [persist](persist.html) | `fun persist(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Persists the state of the provided sessions. |
| [restore](restore.html) | `fun restore(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Restores the session storage state by reading from the latest persisted version. |
| [start](start.html) | `fun start(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts saving the state frequently and automatically. |
| [stop](stop.html) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops saving the state automatically. |

