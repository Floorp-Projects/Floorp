[android-components](../../index.md) / [mozilla.components.feature.app.links](../index.md) / [AppLinksInterceptor](index.md) / [onLoadRequest](./on-load-request.md)

# onLoadRequest

`fun onLoadRequest(engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, hasUserGesture: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, isSameDomain: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`InterceptionResponse`](../../mozilla.components.concept.engine.request/-request-interceptor/-interception-response/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/app-links/src/main/java/mozilla/components/feature/app/links/AppLinksInterceptor.kt#L54)

Overrides [RequestInterceptor.onLoadRequest](../../mozilla.components.concept.engine.request/-request-interceptor/on-load-request.md)

A request to open an URI. This is called before each page load to allow
providing custom behavior.

### Parameters

`engineSession` - The engine session that initiated the callback.

`uri` - The the URI of the request.

`hasUserGesture` - If the request if triggered by the user then true, else false.

`isSameDomain` - If the request is the same domain as the current URL then true, else false.

**Return**
An [InterceptionResponse](../../mozilla.components.concept.engine.request/-request-interceptor/-interception-response/index.md) object containing alternative content
or an alternative URL. Null if the original request should continue to
be loaded.

