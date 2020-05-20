[android-components](../../index.md) / [mozilla.components.support.images.decoder](../index.md) / [ImageDecoder](./index.md)

# ImageDecoder

`interface ImageDecoder` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/images/src/main/java/mozilla/components/support/images/decoder/ImageDecoder.kt#L16)

An image decoder that can decode a [ByteArray](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html) into a [Bitmap](#).

Depending on the image format the decoder may internally decode the [ByteArray](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html) into multiple [Bitmap](#). It is up to
the decoder implementation to return the best [Bitmap](#) to use.

### Types

| Name | Summary |
|---|---|
| [ImageMagicNumbers](-image-magic-numbers/index.md) | `enum class ImageMagicNumbers` |

### Functions

| Name | Summary |
|---|---|
| [decode](decode.md) | `abstract fun decode(data: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`, desiredSize: `[`DesiredSize`](../../mozilla.components.support.images/-desired-size/index.md)`): <ERROR CLASS>?`<br>Decodes the given [data](decode.md#mozilla.components.support.images.decoder.ImageDecoder$decode(kotlin.ByteArray, mozilla.components.support.images.DesiredSize)/data) into a a [Bitmap](#) or null. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [AndroidImageDecoder](../-android-image-decoder/index.md) | `class AndroidImageDecoder : `[`ImageDecoder`](./index.md)<br>[ImageDecoder](./index.md) that will use Android's [BitmapFactory](#) in order to decode the byte data. |
| [ICOIconDecoder](../../mozilla.components.browser.icons.decoder/-i-c-o-icon-decoder/index.md) | `class ICOIconDecoder : `[`ImageDecoder`](./index.md)<br>[ImageDecoder](./index.md) implementation for decoding ICO files. |
