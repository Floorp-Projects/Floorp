[android-components](../index.md) / [mozilla.components.support.images.ext](index.md) / [loadIntoView](./load-into-view.md)

# loadIntoView

`fun `[`ImageLoader`](../mozilla.components.support.images.loader/-image-loader/index.md)`.loadIntoView(view: <ERROR CLASS>, id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/images/src/main/java/mozilla/components/support/images/ext/ImageLoader.kt#L19)

Loads an image asynchronously and then displays it in the [ImageView](#).
If the view is detached from the window before loading is completed, then loading is cancelled.

### Parameters

`view` - [ImageView](#) to load the image into.

`id` - Load image for this given ID.