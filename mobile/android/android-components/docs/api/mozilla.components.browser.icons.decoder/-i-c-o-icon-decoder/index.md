[android-components](../../index.md) / [mozilla.components.browser.icons.decoder](../index.md) / [ICOIconDecoder](./index.md)

# ICOIconDecoder

`class ICOIconDecoder : `[`ImageDecoder`](../../mozilla.components.support.images.decoder/-image-decoder/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/decoder/ICOIconDecoder.kt#L25)

[ImageDecoder](../../mozilla.components.support.images.decoder/-image-decoder/index.md) implementation for decoding ICO files.

An ICO file is a container format that may hold up to 255 images in either BMP or PNG format.
A mixture of image types may not exist.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ICOIconDecoder()`<br>[ImageDecoder](../../mozilla.components.support.images.decoder/-image-decoder/index.md) implementation for decoding ICO files. |

### Functions

| Name | Summary |
|---|---|
| [decode](decode.md) | `fun decode(data: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`, desiredSize: `[`DesiredSize`](../../mozilla.components.support.images/-desired-size/index.md)`): <ERROR CLASS>?`<br>Decodes the given [data](../../mozilla.components.support.images.decoder/-image-decoder/decode.md#mozilla.components.support.images.decoder.ImageDecoder$decode(kotlin.ByteArray, mozilla.components.support.images.DesiredSize)/data) into a a [Bitmap](#) or null. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
