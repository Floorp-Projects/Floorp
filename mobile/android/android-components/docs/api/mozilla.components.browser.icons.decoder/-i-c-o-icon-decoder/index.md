[android-components](../../index.md) / [mozilla.components.browser.icons.decoder](../index.md) / [ICOIconDecoder](./index.md)

# ICOIconDecoder

`class ICOIconDecoder : `[`IconDecoder`](../-icon-decoder/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/decoder/ICOIconDecoder.kt#L23)

[IconDecoder](../-icon-decoder/index.md) implementation for decoding ICO files.

An ICO file is a container format that may hold up to 255 images in either BMP or PNG format.
A mixture of image types may not exist.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ICOIconDecoder()`<br>[IconDecoder](../-icon-decoder/index.md) implementation for decoding ICO files. |

### Functions

| Name | Summary |
|---|---|
| [decode](decode.md) | `fun decode(data: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`, targetSize: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, maxSize: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, maxScaleFactor: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)`): `[`Bitmap`](https://developer.android.com/reference/android/graphics/Bitmap.html)`?`<br>Decodes the given [data](../-icon-decoder/decode.md#mozilla.components.browser.icons.decoder.IconDecoder$decode(kotlin.ByteArray, kotlin.Int, kotlin.Int, kotlin.Float)/data) into a a [Bitmap](https://developer.android.com/reference/android/graphics/Bitmap.html) or null. |
