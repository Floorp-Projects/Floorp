[android-components](../../index.md) / [mozilla.components.feature.app.links](../index.md) / [AppLinkRedirect](./index.md)

# AppLinkRedirect

`data class AppLinkRedirect` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/app-links/src/main/java/mozilla/components/feature/app/links/AppLinkRedirect.kt#L12)

Data class for the external Intent or fallback URL a given URL encodes for.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AppLinkRedirect(appIntent: <ERROR CLASS>?, fallbackUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, marketplaceIntent: <ERROR CLASS>?)`<br>Data class for the external Intent or fallback URL a given URL encodes for. |

### Properties

| Name | Summary |
|---|---|
| [appIntent](app-intent.md) | `val appIntent: <ERROR CLASS>?` |
| [fallbackUrl](fallback-url.md) | `val fallbackUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |
| [marketplaceIntent](marketplace-intent.md) | `val marketplaceIntent: <ERROR CLASS>?` |

### Functions

| Name | Summary |
|---|---|
| [hasExternalApp](has-external-app.md) | `fun hasExternalApp(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If there is a third-party app intent. |
| [hasFallback](has-fallback.md) | `fun hasFallback(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If there is a fallback URL (should the intent fails). |
| [hasMarketplaceIntent](has-marketplace-intent.md) | `fun hasMarketplaceIntent(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If there is a marketplace intent (should the external app is not installed). |
| [isInstallable](is-installable.md) | `fun isInstallable(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Is the app link one that can be installed from a store. |
| [isRedirect](is-redirect.md) | `fun isRedirect(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If the app link is a redirect (to an app or URL). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
