[android-components](../../index.md) / [mozilla.components.browser.icons.decoder](../index.md) / [IconDecoder](index.md) / [decode](./decode.md)

# decode

`abstract fun decode(data: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`, targetSize: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, maxSize: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, maxScaleFactor: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)`): `[`Bitmap`](https://developer.android.com/reference/android/graphics/Bitmap.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/decoder/IconDecoder.kt#L22)

Decodes the given [data](decode.md#mozilla.components.browser.icons.decoder.IconDecoder$decode(kotlin.ByteArray, kotlin.Int, kotlin.Int, kotlin.Float)/data) into a a [Bitmap](https://developer.android.com/reference/android/graphics/Bitmap.html) or null.

The caller provides a maximum size. This is useful for image formats that may decode into multiple images. The
decoder can use this size to determine which [Bitmap](https://developer.android.com/reference/android/graphics/Bitmap.html) to return.

