[android-components](../../index.md) / [mozilla.components.browser.thumbnails](../index.md) / [ThumbnailsMiddleware](./index.md)

# ThumbnailsMiddleware

`class ThumbnailsMiddleware : `[`Middleware`](../../mozilla.components.lib.state/-middleware.md)`<`[`BrowserState`](../../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/thumbnails/src/main/java/mozilla/components/browser/thumbnails/ThumbnailsMiddleware.kt#L19)

[Middleware](../../mozilla.components.lib.state/-middleware.md) implementation for handling [ContentAction.UpdateThumbnailAction](../../mozilla.components.browser.state.action/-content-action/-update-thumbnail-action/index.md) and storing
the thumbnail to the disk cache.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ThumbnailsMiddleware(thumbnailStorage: `[`ThumbnailStorage`](../../mozilla.components.browser.thumbnails.storage/-thumbnail-storage/index.md)`)`<br>[Middleware](../../mozilla.components.lib.state/-middleware.md) implementation for handling [ContentAction.UpdateThumbnailAction](../../mozilla.components.browser.state.action/-content-action/-update-thumbnail-action/index.md) and storing the thumbnail to the disk cache. |

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `fun invoke(store: `[`MiddlewareStore`](../../mozilla.components.lib.state/-middleware-store/index.md)`<`[`BrowserState`](../../mozilla.components.browser.state.state/-browser-state/index.md)`, `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`>, next: (`[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, action: `[`BrowserAction`](../../mozilla.components.browser.state.action/-browser-action.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
