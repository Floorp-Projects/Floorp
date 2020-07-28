[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [WebAppInterceptor](./index.md)

# WebAppInterceptor

`class WebAppInterceptor : `[`RequestInterceptor`](../../mozilla.components.concept.engine.request/-request-interceptor/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/WebAppInterceptor.kt#L20)

This feature will intercept requests and reopen them in the corresponding installed PWA, if any.

### Parameters

`shortcutManager` - current shortcut manager instance to lookup web app install states

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebAppInterceptor(context: <ERROR CLASS>, manifestStorage: `[`ManifestStorage`](../-manifest-storage/index.md)`, launchFromInterceptor: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)`<br>This feature will intercept requests and reopen them in the corresponding installed PWA, if any. |

### Functions

| Name | Summary |
|---|---|
| [onLoadRequest](on-load-request.md) | `fun onLoadRequest(engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, lastUri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, hasUserGesture: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, isSameDomain: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, isRedirect: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, isDirectNavigation: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, isSubframeRequest: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`InterceptionResponse`](../../mozilla.components.concept.engine.request/-request-interceptor/-interception-response/index.md)`?`<br>A request to open an URI. This is called before each page load to allow providing custom behavior. |

### Inherited Functions

| Name | Summary |
|---|---|
| [interceptsAppInitiatedRequests](../../mozilla.components.concept.engine.request/-request-interceptor/intercepts-app-initiated-requests.md) | `open fun interceptsAppInitiatedRequests(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether or not this [RequestInterceptor](../../mozilla.components.concept.engine.request/-request-interceptor/index.md) should intercept load requests initiated by the app (via direct calls to [EngineSession.loadUrl](../../mozilla.components.concept.engine/-engine-session/load-url.md)). All other requests triggered by users interacting with web content (e.g. following links) or redirects will always be intercepted. |
| [onErrorRequest](../../mozilla.components.concept.engine.request/-request-interceptor/on-error-request.md) | `open fun onErrorRequest(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, errorType: `[`ErrorType`](../../mozilla.components.browser.errorpages/-error-type/index.md)`, uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`ErrorResponse`](../../mozilla.components.concept.engine.request/-request-interceptor/-error-response/index.md)`?`<br>A request that the engine wasn't able to handle that resulted in an error. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
