[android-components](../index.md) / [mozilla.components.browser.thumbnails.ext](index.md) / [loadIntoView](./load-into-view.md)

# loadIntoView

`fun `[`ThumbnailLoader`](../mozilla.components.browser.thumbnails.loader/-thumbnail-loader/index.md)`.loadIntoView(view: <ERROR CLASS>, id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/thumbnails/src/main/java/mozilla/components/browser/thumbnails/ext/ThumbnailLoader.kt#L18)

Loads an image asynchronously and then displays it in the [ImageView](#).
If the view is detached from the window before loading is completed, then loading is cancelled.

### Parameters

`view` - [ImageView](#) to load the image into.

`id` - Load image for this given ID.