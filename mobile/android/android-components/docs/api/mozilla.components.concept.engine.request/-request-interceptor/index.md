[android-components](../../index.md) / [mozilla.components.concept.engine.request](../index.md) / [RequestInterceptor](./index.md)

# RequestInterceptor

`interface RequestInterceptor` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/request/RequestInterceptor.kt#L13)

Interface for classes that want to intercept load requests to allow custom behavior.

### Types

| Name | Summary |
|---|---|
| [ErrorResponse](-error-response/index.md) | `data class ErrorResponse`<br>An alternative response for an error request. |
| [InterceptionResponse](-interception-response/index.md) | `sealed class InterceptionResponse`<br>An alternative response for an intercepted request. |

### Functions

| Name | Summary |
|---|---|
| [onErrorRequest](on-error-request.md) | `open fun onErrorRequest(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, errorType: `[`ErrorType`](../../mozilla.components.browser.errorpages/-error-type/index.md)`, uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`ErrorResponse`](-error-response/index.md)`?`<br>A request that the engine wasn't able to handle that resulted in an error. |
| [onLoadRequest](on-load-request.md) | `open fun onLoadRequest(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`InterceptionResponse`](-interception-response/index.md)`?`<br>A request to open an URI. This is called before each page load to allow providing custom behavior. |
