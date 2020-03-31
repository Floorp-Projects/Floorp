[android-components](../../index.md) / [mozilla.components.concept.engine.request](../index.md) / [RequestInterceptor](index.md) / [onLoadRequest](./on-load-request.md)

# onLoadRequest

`open fun onLoadRequest(engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, hasUserGesture: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, isSameDomain: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`InterceptionResponse`](-interception-response/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/request/RequestInterceptor.kt#L63)

A request to open an URI. This is called before each page load to allow
providing custom behavior.

### Parameters

`engineSession` - The engine session that initiated the callback.

`uri` - The the URI of the request.

`hasUserGesture` - If the request if triggered by the user then true, else false.

`isSameDomain` - If the request is the same domain as the current URL then true, else false.

**Return**
An [InterceptionResponse](-interception-response/index.md) object containing alternative content
or an alternative URL. Null if the original request should continue to
be loaded.

