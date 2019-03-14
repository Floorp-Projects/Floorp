[android-components](../../index.md) / [mozilla.components.browser.engine.gecko](../index.md) / [GeckoEngineView](index.md) / [captureThumbnail](./capture-thumbnail.md)

# captureThumbnail

`fun captureThumbnail(onFinish: (`[`Bitmap`](https://developer.android.com/reference/android/graphics/Bitmap.html)`?) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/GeckoEngineView.kt#L63)

Overrides [EngineView.captureThumbnail](../../mozilla.components.concept.engine/-engine-view/capture-thumbnail.md)

Request a screenshot of the visible portion of the web page currently being rendered.

### Parameters

`onFinish` - A callback to inform that process of capturing a thumbnail has finished.