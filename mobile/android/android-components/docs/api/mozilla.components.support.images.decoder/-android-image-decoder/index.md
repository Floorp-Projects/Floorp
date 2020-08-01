[android-components](../../index.md) / [mozilla.components.support.images.decoder](../index.md) / [AndroidImageDecoder](./index.md)

# AndroidImageDecoder

`class AndroidImageDecoder : `[`ImageDecoder`](../-image-decoder/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/images/src/main/java/mozilla/components/support/images/decoder/AndroidImageDecoder.kt#L21)

[ImageDecoder](../-image-decoder/index.md) that will use Android's [BitmapFactory](#) in order to decode the byte data.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AndroidImageDecoder()`<br>[ImageDecoder](../-image-decoder/index.md) that will use Android's [BitmapFactory](#) in order to decode the byte data. |

### Functions

| Name | Summary |
|---|---|
| [decode](decode.md) | `fun decode(data: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`, desiredSize: `[`DesiredSize`](../../mozilla.components.support.images/-desired-size/index.md)`): <ERROR CLASS>?`<br>Decodes the given [data](../-image-decoder/decode.md#mozilla.components.support.images.decoder.ImageDecoder$decode(kotlin.ByteArray, mozilla.components.support.images.DesiredSize)/data) into a a [Bitmap](#) or null. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
