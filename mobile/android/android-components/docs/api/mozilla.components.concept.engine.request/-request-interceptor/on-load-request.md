[android-components](../../index.md) / [mozilla.components.concept.engine.request](../index.md) / [RequestInterceptor](index.md) / [onLoadRequest](./on-load-request.md)

# onLoadRequest

`open fun onLoadRequest(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`InterceptionResponse`](-interception-response/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/request/RequestInterceptor.kt#L46)

A request to open an URI. This is called before each page load to allow
providing custom behavior.

### Parameters

`session` - The engine session that initiated the callback.

**Return**
An [InterceptionResponse](-interception-response/index.md) object containing alternative content
or an alternative URL. Null if the original request should continue to
be loaded.

