[android-components](../../index.md) / [mozilla.components.feature.app.links](../index.md) / [AppLinksUseCases](./index.md)

# AppLinksUseCases

`class AppLinksUseCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/app-links/src/main/java/mozilla/components/feature/app/links/AppLinksUseCases.kt#L30)

These use cases allow for the detection of, and opening of links that other apps have registered
an [IntentFilter](#)s to open.

Care is taken to:

* resolve [intent//](intent//) links, including [S.browser_fallback_url](#)
* provide a fallback to the installed marketplace app (e.g. on Google Android, the Play Store).
* open HTTP(S) links with an external app.

Since browsers are able to open HTTPS pages, existing browser apps are excluded from the list of
apps that trigger a redirect to an external app.

### Types

| Name | Summary |
|---|---|
| [GetAppLinkRedirect](-get-app-link-redirect/index.md) | `inner class GetAppLinkRedirect`<br>Parse a URL and check if it can be handled by an app elsewhere on the Android device. If that app is not available, then a market place intent is also provided. |
| [OpenAppLinkRedirect](-open-app-link-redirect/index.md) | `class OpenAppLinkRedirect`<br>Open an external app with the redirect created by the [GetAppLinkRedirect](-get-app-link-redirect/index.md). |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AppLinksUseCases(context: <ERROR CLASS>, browserPackageNames: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>? = null, unguessableWebUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "https://${UUID.randomUUID()}.net")`<br>These use cases allow for the detection of, and opening of links that other apps have registered an [IntentFilter](#)s to open. |

### Properties

| Name | Summary |
|---|---|
| [appLinkRedirect](app-link-redirect.md) | `val appLinkRedirect: `[`GetAppLinkRedirect`](-get-app-link-redirect/index.md) |
| [browserPackageNames](browser-package-names.md) | `val browserPackageNames: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |
| [interceptedAppLinkRedirect](intercepted-app-link-redirect.md) | `val interceptedAppLinkRedirect: `[`GetAppLinkRedirect`](-get-app-link-redirect/index.md) |
| [openAppLink](open-app-link.md) | `val openAppLink: `[`OpenAppLinkRedirect`](-open-app-link-redirect/index.md) |
