[android-components](../../index.md) / [mozilla.components.feature.app.links](../index.md) / [AppLinksInterceptor](./index.md)

# AppLinksInterceptor

`class AppLinksInterceptor : `[`RequestInterceptor`](../../mozilla.components.concept.engine.request/-request-interceptor/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/app-links/src/main/java/mozilla/components/feature/app/links/AppLinksInterceptor.kt#L50)

This feature implements use cases for detecting and handling redirects to external apps. The user
is asked to confirm her intention before leaving the app. These include the Android Intents,
custom schemes and support for [Intent.CATEGORY_BROWSABLE](#) `http(s)` URLs.

In the case of Android Intents that are not installed, and with no fallback, the user is prompted
to search the installed market place.

It provides use cases to detect and open links openable in third party non-browser apps.

It requires: a [Context](#).

A [Boolean](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) flag is provided at construction to allow the feature and use cases to be landed without
adjoining UI. The UI will be activated in https://github.com/mozilla-mobile/android-components/issues/2974
and https://github.com/mozilla-mobile/android-components/issues/2975.

### Parameters

`context` - Context the feature is associated with.

`interceptLinkClicks` - If {true} then intercept link clicks.

`engineSupportedSchemes` - List of schemes that the engine supports.

`alwaysDeniedSchemes` - List of schemes that will never be opened in a third-party app even if
[interceptLinkClicks](#) is `true`.

`launchInApp` - If {true} then launch app links in third party app(s). Default to false because
of security concerns.

`useCases` - These use cases allow for the detection of, and opening of links that other apps
have registered to open.

`launchFromInterceptor` - If {true} then the interceptor will launch the link in third-party apps if available.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AppLinksInterceptor(context: <ERROR CLASS>, interceptLinkClicks: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, engineSupportedSchemes: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = ENGINE_SUPPORTED_SCHEMES, alwaysDeniedSchemes: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = ALWAYS_DENY_SCHEMES, launchInApp: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { false }, useCases: `[`AppLinksUseCases`](../-app-links-use-cases/index.md)` = AppLinksUseCases(context, launchInApp,
        alwaysDeniedSchemes = alwaysDeniedSchemes), launchFromInterceptor: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`<br>This feature implements use cases for detecting and handling redirects to external apps. The user is asked to confirm her intention before leaving the app. These include the Android Intents, custom schemes and support for [Intent.CATEGORY_BROWSABLE](#) `http(s)` URLs. |

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
