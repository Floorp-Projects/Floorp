[android-components](../../index.md) / [mozilla.components.feature.app.links](../index.md) / [AppLinksInterceptor](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`AppLinksInterceptor(context: <ERROR CLASS>, interceptLinkClicks: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, engineSupportedSchemes: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = ENGINE_SUPPORTED_SCHEMES, alwaysDeniedSchemes: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = ALWAYS_DENY_SCHEMES, launchInApp: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { false }, useCases: `[`AppLinksUseCases`](../-app-links-use-cases/index.md)` = AppLinksUseCases(context, launchInApp,
        alwaysDeniedSchemes = alwaysDeniedSchemes), launchFromInterceptor: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`

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