[android-components](../../index.md) / [mozilla.components.browser.thumbnails.loader](../index.md) / [ThumbnailLoader](./index.md)

# ThumbnailLoader

`class ThumbnailLoader : `[`ImageLoader`](../../mozilla.components.support.images.loader/-image-loader/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/thumbnails/src/main/java/mozilla/components/browser/thumbnails/loader/ThumbnailLoader.kt#L25)

An implementation of [ImageLoader](../../mozilla.components.support.images.loader/-image-loader/index.md) for loading thumbnails into a [ImageView](#).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ThumbnailLoader(storage: `[`ThumbnailStorage`](../../mozilla.components.browser.thumbnails.storage/-thumbnail-storage/index.md)`)`<br>An implementation of [ImageLoader](../../mozilla.components.support.images.loader/-image-loader/index.md) for loading thumbnails into a [ImageView](#). |

### Functions

| Name | Summary |
|---|---|
| [loadIntoView](load-into-view.md) | `fun loadIntoView(view: <ERROR CLASS>, request: `[`ImageLoadRequest`](../../mozilla.components.support.images/-image-load-request/index.md)`, placeholder: <ERROR CLASS>?, error: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Loads an image asynchronously and then displays it in the [ImageView](#). If the view is detached from the window before loading is completed, then loading is cancelled. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
