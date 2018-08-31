---
title: RequestInterceptor.onLoadRequest - 
---

[mozilla.components.concept.engine.request](../index.html) / [RequestInterceptor](index.html) / [onLoadRequest](./on-load-request.html)

# onLoadRequest

`abstract fun onLoadRequest(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.html)`, uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`InterceptionResponse`](-interception-response/index.html)`?`

A request to open an URI. This is called before each page load to allow custom behavior implementation.

### Parameters

`session` - The engine session that initiated the callback.

**Return**
An InterceptionResponse object containing alternative content if the request should be intercepted.
    null otherwise.

