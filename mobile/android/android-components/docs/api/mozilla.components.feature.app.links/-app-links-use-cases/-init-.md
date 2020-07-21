[android-components](../../index.md) / [mozilla.components.feature.app.links](../index.md) / [AppLinksUseCases](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`AppLinksUseCases(context: <ERROR CLASS>, launchInApp: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { false }, alwaysDeniedSchemes: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = ALWAYS_DENY_SCHEMES)`

These use cases allow for the detection of, and opening of links that other apps have registered
an [IntentFilter](#)s to open.

Care is taken to:

* resolve [intent//](intent//) links, including [S.browser_fallback_url](#)
* provide a fallback to the installed marketplace app (e.g. on Google Android, the Play Store).
* open HTTP(S) links with an external app.

Since browsers are able to open HTTPS pages, existing browser apps are excluded from the list of
apps that trigger a redirect to an external app.

### Parameters

`context` - Context the feature is associated with.

`launchInApp` - If {true} then launch app links in third party app(s). Default to false because
of security concerns.

`alwaysDeniedSchemes` - List of schemes that will never be opened in a third-party app.