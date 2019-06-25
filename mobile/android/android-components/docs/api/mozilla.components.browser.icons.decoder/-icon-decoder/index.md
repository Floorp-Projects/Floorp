[android-components](../../index.md) / [mozilla.components.browser.icons.decoder](../index.md) / [IconDecoder](./index.md)

# IconDecoder

`interface IconDecoder` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/decoder/IconDecoder.kt#L16)

An icon decoder that can decode a [ByteArray](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html) into a [Bitmap](#).

Depending on the image format the decoder may internally decode the [ByteArray](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html) into multiple [Bitmap](#). It is up to
the decoder implementation to return the best [Bitmap](#) to use.

### Types

| Name | Summary |
|---|---|
| [ImageMagicNumbers](-image-magic-numbers/index.md) | `enum class ImageMagicNumbers` |

### Functions

| Name | Summary |
|---|---|
| [decode](decode.md) | `abstract fun decode(data: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`, desiredSize: `[`DesiredSize`](../../mozilla.components.browser.icons/-desired-size/index.md)`): <ERROR CLASS>?`<br>Decodes the given [data](decode.md#mozilla.components.browser.icons.decoder.IconDecoder$decode(kotlin.ByteArray, mozilla.components.browser.icons.DesiredSize)/data) into a a [Bitmap](#) or null. |

### Inheritors

| Name | Summary |
|---|---|
| [AndroidIconDecoder](../-android-icon-decoder/index.md) | `class AndroidIconDecoder : `[`IconDecoder`](./index.md)<br>[IconDecoder](./index.md) that will use Android's [BitmapFactory](#) in order to decode the byte data. |
| [ICOIconDecoder](../-i-c-o-icon-decoder/index.md) | `class ICOIconDecoder : `[`IconDecoder`](./index.md)<br>[IconDecoder](./index.md) implementation for decoding ICO files. |
