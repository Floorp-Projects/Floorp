[android-components](../../index.md) / [mozilla.components.browser.icons.decoder](../index.md) / [AndroidIconDecoder](./index.md)

# AndroidIconDecoder

`class AndroidIconDecoder : `[`IconDecoder`](../-icon-decoder/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/decoder/AndroidIconDecoder.kt#L16)

[IconDecoder](../-icon-decoder/index.md) that will use Android's [BitmapFactory](https://developer.android.com/reference/android/graphics/BitmapFactory.html) in order to decode the byte data.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AndroidIconDecoder()`<br>[IconDecoder](../-icon-decoder/index.md) that will use Android's [BitmapFactory](https://developer.android.com/reference/android/graphics/BitmapFactory.html) in order to decode the byte data. |

### Functions

| Name | Summary |
|---|---|
| [decode](decode.md) | `fun decode(data: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`, targetSize: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, maxSize: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, maxScaleFactor: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)`): `[`Bitmap`](https://developer.android.com/reference/android/graphics/Bitmap.html)`?`<br>Decodes the given [data](../-icon-decoder/decode.md#mozilla.components.browser.icons.decoder.IconDecoder$decode(kotlin.ByteArray, kotlin.Int, kotlin.Int, kotlin.Float)/data) into a a [Bitmap](https://developer.android.com/reference/android/graphics/Bitmap.html) or null. |
