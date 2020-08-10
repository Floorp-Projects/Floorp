[android-components](../../index.md) / [mozilla.components.feature.downloads](../index.md) / [DownloadMiddleware](./index.md)

# DownloadMiddleware

`class DownloadMiddleware : `[`Middleware`](../../mozilla.components.lib.state/-middleware.md)`<`[`BrowserState`](../../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/DownloadMiddleware.kt#L21)

[Middleware](../../mozilla.components.lib.state/-middleware.md) implementation for managing downloads via the provided download service. Its
purpose is to react to global download state changes (e.g. of [BrowserState.downloads](../../mozilla.components.browser.state.state/-browser-state/downloads.md))
and notify the download service, as needed.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DownloadMiddleware(applicationContext: <ERROR CLASS>, downloadServiceClass: `[`Class`](http://docs.oracle.com/javase/7/docs/api/java/lang/Class.html)`<*>)`<br>[Middleware](../../mozilla.components.lib.state/-middleware.md) implementation for managing downloads via the provided download service. Its purpose is to react to global download state changes (e.g. of [BrowserState.downloads](../../mozilla.components.browser.state.state/-browser-state/downloads.md)) and notify the download service, as needed. |

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `fun invoke(store: `[`MiddlewareStore`](../../mozilla.components.lib.state/-middleware-store/index.md)`<`[`BrowserState`](../../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`>, next: (`[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, action: `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
