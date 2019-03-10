[android-components](../../index.md) / [mozilla.components.browser.icons.decoder](../index.md) / [IconDecoder](./index.md)

# IconDecoder

`interface IconDecoder` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/decoder/IconDecoder.kt#L15)

An icon decoder that can decode a [ByteArray](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html) into a [Bitmap](https://developer.android.com/reference/android/graphics/Bitmap.html).

Depending on the image format the decoder may internally decode the [ByteArray](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html) into multiple [Bitmap](https://developer.android.com/reference/android/graphics/Bitmap.html). It is up to
the decoder implementation to return the best [Bitmap](https://developer.android.com/reference/android/graphics/Bitmap.html) to use.

### Types

| Name | Summary |
|---|---|
| [ImageMagicNumbers](-image-magic-numbers/index.md) | `enum class ImageMagicNumbers` |

### Functions

| Name | Summary |
|---|---|
| [decode](decode.md) | `abstract fun decode(data: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`, targetSize: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, maxSize: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, maxScaleFactor: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)`): `[`Bitmap`](https://developer.android.com/reference/android/graphics/Bitmap.html)`?`<br>Decodes the given [data](decode.md#mozilla.components.browser.icons.decoder.IconDecoder$decode(kotlin.ByteArray, kotlin.Int, kotlin.Int, kotlin.Float)/data) into a a [Bitmap](https://developer.android.com/reference/android/graphics/Bitmap.html) or null. |

### Inheritors

| Name | Summary |
|---|---|
| [ICOIconDecoder](../-i-c-o-icon-decoder/index.md) | `class ICOIconDecoder : `[`IconDecoder`](./index.md)<br>[IconDecoder](./index.md) implementation for decoding ICO files. |
