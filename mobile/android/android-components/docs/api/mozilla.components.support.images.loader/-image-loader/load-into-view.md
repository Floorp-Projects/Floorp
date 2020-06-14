[android-components](../../index.md) / [mozilla.components.support.images.loader](../index.md) / [ImageLoader](index.md) / [loadIntoView](./load-into-view.md)

# loadIntoView

`@MainThread abstract fun loadIntoView(view: <ERROR CLASS>, id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, placeholder: <ERROR CLASS>? = null, error: <ERROR CLASS>? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/images/src/main/java/mozilla/components/support/images/loader/ImageLoader.kt#L26)

Loads an image asynchronously and then displays it in the [ImageView](#).
If the view is detached from the window before loading is completed, then loading is cancelled.

### Parameters

`view` - [ImageView](#) to load the image into.

`id` - Load image for this given ID.

`placeholder` - [Drawable](#) to display while image is loading.

`error` - [Drawable](#) to display if loading fails.