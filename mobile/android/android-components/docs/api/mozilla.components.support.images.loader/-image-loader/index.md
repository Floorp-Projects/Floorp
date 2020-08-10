[android-components](../../index.md) / [mozilla.components.support.images.loader](../index.md) / [ImageLoader](./index.md)

# ImageLoader

`interface ImageLoader` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/images/src/main/java/mozilla/components/support/images/loader/ImageLoader.kt#L15)

A loader that can load an image from an ID directly into an [ImageView](#).

### Functions

| Name | Summary |
|---|---|
| [loadIntoView](load-into-view.md) | `abstract fun loadIntoView(view: <ERROR CLASS>, request: `[`ImageLoadRequest`](../../mozilla.components.support.images/-image-load-request/index.md)`, placeholder: <ERROR CLASS>? = null, error: <ERROR CLASS>? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Loads an image asynchronously and then displays it in the [ImageView](#). If the view is detached from the window before loading is completed, then loading is cancelled. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [ThumbnailLoader](../../mozilla.components.browser.thumbnails.loader/-thumbnail-loader/index.md) | `class ThumbnailLoader : `[`ImageLoader`](./index.md)<br>An implementation of [ImageLoader](./index.md) for loading thumbnails into a [ImageView](#). |
