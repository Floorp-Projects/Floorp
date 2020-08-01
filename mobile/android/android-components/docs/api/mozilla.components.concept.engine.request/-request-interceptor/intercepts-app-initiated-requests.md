[android-components](../../index.md) / [mozilla.components.concept.engine.request](../index.md) / [RequestInterceptor](index.md) / [interceptsAppInitiatedRequests](./intercepts-app-initiated-requests.md)

# interceptsAppInitiatedRequests

`open fun interceptsAppInitiatedRequests(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/request/RequestInterceptor.kt#L105)

Returns whether or not this [RequestInterceptor](index.md) should intercept load
requests initiated by the app (via direct calls to [EngineSession.loadUrl](../../mozilla.components.concept.engine/-engine-session/load-url.md)).
All other requests triggered by users interacting with web content
(e.g. following links) or redirects will always be intercepted.

**Return**
true if app initiated requests should be intercepted,
otherwise false. Defaults to false.

