[android-components](../../index.md) / [mozilla.components.feature.app.links](../index.md) / [AppLinksUseCases](./index.md)

# AppLinksUseCases

`class AppLinksUseCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/app-links/src/main/java/mozilla/components/feature/app/links/AppLinksUseCases.kt#L46)

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

### Types

| Name | Summary |
|---|---|
| [GetAppLinkRedirect](-get-app-link-redirect/index.md) | `inner class GetAppLinkRedirect`<br>Parse a URL and check if it can be handled by an app elsewhere on the Android device. If that app is not available, then a market place intent is also provided. |
| [OpenAppLinkRedirect](-open-app-link-redirect/index.md) | `inner class OpenAppLinkRedirect`<br>Open an external app with the redirect created by the [GetAppLinkRedirect](-get-app-link-redirect/index.md). |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AppLinksUseCases(context: <ERROR CLASS>, launchInApp: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { false }, alwaysDeniedSchemes: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = ALWAYS_DENY_SCHEMES)`<br>These use cases allow for the detection of, and opening of links that other apps have registered an [IntentFilter](#)s to open. |

### Properties

| Name | Summary |
|---|---|
| [appLinkRedirect](app-link-redirect.md) | `val appLinkRedirect: `[`GetAppLinkRedirect`](-get-app-link-redirect/index.md) |
| [appLinkRedirectIncludeInstall](app-link-redirect-include-install.md) | `val appLinkRedirectIncludeInstall: `[`GetAppLinkRedirect`](-get-app-link-redirect/index.md) |
| [interceptedAppLinkRedirect](intercepted-app-link-redirect.md) | `val interceptedAppLinkRedirect: `[`GetAppLinkRedirect`](-get-app-link-redirect/index.md) |
| [openAppLink](open-app-link.md) | `val openAppLink: `[`OpenAppLinkRedirect`](-open-app-link-redirect/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
