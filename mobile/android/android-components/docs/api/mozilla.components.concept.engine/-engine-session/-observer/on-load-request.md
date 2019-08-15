[android-components](../../../index.md) / [mozilla.components.concept.engine](../../index.md) / [EngineSession](../index.md) / [Observer](index.md) / [onLoadRequest](./on-load-request.md)

# onLoadRequest

`open fun onLoadRequest(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, triggeredByRedirect: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, triggeredByWebContent: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L72)

The engine received a request to load a request.

### Parameters

`url` - The string url that was requested.

`triggeredByRedirect` - True if and only if the request was triggered by an HTTP redirect.

`triggeredByWebContent` - True if and only if the request was triggered from within
web content (as opposed to via the browser chrome).