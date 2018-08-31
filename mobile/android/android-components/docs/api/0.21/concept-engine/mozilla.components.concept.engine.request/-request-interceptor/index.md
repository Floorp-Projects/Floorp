---
title: RequestInterceptor - 
---

[mozilla.components.concept.engine.request](../index.html) / [RequestInterceptor](./index.html)

# RequestInterceptor

`interface RequestInterceptor`

Interface for classes that want to intercept load requests to allow custom behavior.

### Types

| [InterceptionResponse](-interception-response/index.html) | `data class InterceptionResponse`<br>An alternative response for an intercepted request. |

### Functions

| [onLoadRequest](on-load-request.html) | `abstract fun onLoadRequest(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.html)`, uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`InterceptionResponse`](-interception-response/index.html)`?`<br>A request to open an URI. This is called before each page load to allow custom behavior implementation. |

