[android-components](../../index.md) / [mozilla.components.feature.app.links](../index.md) / [AppLinkRedirect](./index.md)

# AppLinkRedirect

`data class AppLinkRedirect` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/app-links/src/main/java/mozilla/components/feature/app/links/AppLinkRedirect.kt#L13)

Data class for the external Intent or fallback URL a given URL encodes for.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AppLinkRedirect(appIntent: <ERROR CLASS>?, webUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, isFallback: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, info: <ERROR CLASS>? = null)`<br>Data class for the external Intent or fallback URL a given URL encodes for. |

### Properties

| Name | Summary |
|---|---|
| [appIntent](app-intent.md) | `val appIntent: <ERROR CLASS>?` |
| [info](info.md) | `val info: <ERROR CLASS>?` |
| [isFallback](is-fallback.md) | `val isFallback: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [webUrl](web-url.md) | `val webUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |

### Functions

| Name | Summary |
|---|---|
| [hasExternalApp](has-external-app.md) | `fun hasExternalApp(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If there is a third-party app intent. |
| [hasFallback](has-fallback.md) | `fun hasFallback(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If there is a fallback URL (should the intent fails). |
| [isInstallable](is-installable.md) | `fun isInstallable(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Is the app link one that can be installed from a store. |
| [isRedirect](is-redirect.md) | `fun isRedirect(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If the app link is a redirect (to an app or URL). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
