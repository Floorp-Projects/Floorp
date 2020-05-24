[android-components](../../index.md) / [mozilla.components.feature.media.middleware](../index.md) / [MediaMiddleware](./index.md)

# MediaMiddleware

`class MediaMiddleware : `[`Middleware`](../../mozilla.components.lib.state/-middleware.md)`<`[`BrowserState`](../../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/media/src/main/java/mozilla/components/feature/media/middleware/MediaMiddleware.kt#L28)

[Middleware](../../mozilla.components.lib.state/-middleware.md) implementation for updating the [MediaState.Aggregate](../../mozilla.components.browser.state.state/-media-state/-aggregate/index.md) and launching an
[MediaServiceLauncher](#)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `MediaMiddleware(context: <ERROR CLASS>, mediaServiceClass: `[`Class`](http://docs.oracle.com/javase/7/docs/api/java/lang/Class.html)`<*>)`<br>[Middleware](../../mozilla.components.lib.state/-middleware.md) implementation for updating the [MediaState.Aggregate](../../mozilla.components.browser.state.state/-media-state/-aggregate/index.md) and launching an [MediaServiceLauncher](#) |

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `fun invoke(store: `[`MiddlewareStore`](../../mozilla.components.lib.state/-middleware-store/index.md)`<`[`BrowserState`](../../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`>, next: (`[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, action: `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
