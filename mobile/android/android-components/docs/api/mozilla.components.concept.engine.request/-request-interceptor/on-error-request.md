[android-components](../../index.md) / [mozilla.components.concept.engine.request](../index.md) / [RequestInterceptor](index.md) / [onErrorRequest](./on-error-request.md)

# onErrorRequest

`open fun onErrorRequest(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, errorType: `[`ErrorType`](../../mozilla.components.browser.errorpages/-error-type/index.md)`, uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`ErrorResponse`](-error-response/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/request/RequestInterceptor.kt#L94)

A request that the engine wasn't able to handle that resulted in an error.

### Parameters

`session` - The engine session that initiated the callback.

`errorType` - The error that was provided by the engine related to the
type of error caused.

`uri` - The uri that resulted in the error.

**Return**
An [ErrorResponse](-error-response/index.md) object containing content to display for the
provided error type.

