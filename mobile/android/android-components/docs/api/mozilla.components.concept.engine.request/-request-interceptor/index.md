[android-components](../../index.md) / [mozilla.components.concept.engine.request](../index.md) / [RequestInterceptor](./index.md)

# RequestInterceptor

`interface RequestInterceptor` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/request/RequestInterceptor.kt#L14)

Interface for classes that want to intercept load requests to allow custom behavior.

### Types

| Name | Summary |
|---|---|
| [ErrorResponse](-error-response/index.md) | `sealed class ErrorResponse`<br>An alternative response for an error request. |
| [InterceptionResponse](-interception-response/index.md) | `sealed class InterceptionResponse`<br>An alternative response for an intercepted request. |

### Functions

| Name | Summary |
|---|---|
| [interceptsAppInitiatedRequests](intercepts-app-initiated-requests.md) | `open fun interceptsAppInitiatedRequests(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether or not this [RequestInterceptor](./index.md) should intercept load requests initiated by the app (via direct calls to [EngineSession.loadUrl](../../mozilla.components.concept.engine/-engine-session/load-url.md)). All other requests triggered by users interacting with web content (e.g. following links) or redirects will always be intercepted. |
| [onErrorRequest](on-error-request.md) | `open fun onErrorRequest(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, errorType: `[`ErrorType`](../../mozilla.components.browser.errorpages/-error-type/index.md)`, uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`ErrorResponse`](-error-response/index.md)`?`<br>A request that the engine wasn't able to handle that resulted in an error. |
| [onLoadRequest](on-load-request.md) | `open fun onLoadRequest(engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, lastUri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, hasUserGesture: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, isSameDomain: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, isRedirect: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, isDirectNavigation: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, isSubframeRequest: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`InterceptionResponse`](-interception-response/index.md)`?`<br>A request to open an URI. This is called before each page load to allow providing custom behavior. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [AppLinksInterceptor](../../mozilla.components.feature.app.links/-app-links-interceptor/index.md) | `class AppLinksInterceptor : `[`RequestInterceptor`](./index.md)<br>This feature implements use cases for detecting and handling redirects to external apps. The user is asked to confirm her intention before leaving the app. These include the Android Intents, custom schemes and support for [Intent.CATEGORY_BROWSABLE](#) `http(s)` URLs. |
| [WebAppInterceptor](../../mozilla.components.feature.pwa/-web-app-interceptor/index.md) | `class WebAppInterceptor : `[`RequestInterceptor`](./index.md)<br>This feature will intercept requests and reopen them in the corresponding installed PWA, if any. |
