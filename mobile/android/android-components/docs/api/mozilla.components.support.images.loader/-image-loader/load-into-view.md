[android-components](../../index.md) / [mozilla.components.support.images.loader](../index.md) / [ImageLoader](index.md) / [loadIntoView](./load-into-view.md)

# loadIntoView

`@MainThread abstract fun loadIntoView(view: <ERROR CLASS>, request: `[`ImageLoadRequest`](../../mozilla.components.support.images/-image-load-request/index.md)`, placeholder: <ERROR CLASS>? = null, error: <ERROR CLASS>? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/images/src/main/java/mozilla/components/support/images/loader/ImageLoader.kt#L27)

Loads an image asynchronously and then displays it in the [ImageView](#).
If the view is detached from the window before loading is completed, then loading is cancelled.

### Parameters

`view` - [ImageView](#) to load the image into.

`request` - [ImageLoadRequest](../../mozilla.components.support.images/-image-load-request/index.md) Load image for this given request.

`placeholder` - [Drawable](#) to display while image is loading.

`error` - [Drawable](#) to display if loading fails.