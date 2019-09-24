[android-components](../../index.md) / [mozilla.components.feature.app.links](../index.md) / [AppLinkRedirect](./index.md)

# AppLinkRedirect

`data class AppLinkRedirect` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/app-links/src/main/java/mozilla/components/feature/app/links/AppLinkRedirect.kt#L12)

Data class for the external Intent or fallback URL a given URL encodes for.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AppLinkRedirect(appIntent: <ERROR CLASS>?, webUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, isFallback: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`)`<br>Data class for the external Intent or fallback URL a given URL encodes for. |

### Properties

| Name | Summary |
|---|---|
| [appIntent](app-intent.md) | `val appIntent: <ERROR CLASS>?` |
| [isFallback](is-fallback.md) | `val isFallback: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [webUrl](web-url.md) | `val webUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |

### Functions

| Name | Summary |
|---|---|
| [hasExternalApp](has-external-app.md) | `fun hasExternalApp(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [hasFallback](has-fallback.md) | `fun hasFallback(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [isRedirect](is-redirect.md) | `fun isRedirect(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
