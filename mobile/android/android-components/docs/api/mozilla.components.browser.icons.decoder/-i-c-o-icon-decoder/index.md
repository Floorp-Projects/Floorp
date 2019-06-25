[android-components](../../index.md) / [mozilla.components.browser.icons.decoder](../index.md) / [ICOIconDecoder](./index.md)

# ICOIconDecoder

`class ICOIconDecoder : `[`IconDecoder`](../-icon-decoder/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/decoder/ICOIconDecoder.kt#L24)

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
| [decode](decode.md) | `fun decode(data: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`, desiredSize: `[`DesiredSize`](../../mozilla.components.browser.icons/-desired-size/index.md)`): <ERROR CLASS>?`<br>Decodes the given [data](../-icon-decoder/decode.md#mozilla.components.browser.icons.decoder.IconDecoder$decode(kotlin.ByteArray, mozilla.components.browser.icons.DesiredSize)/data) into a a [Bitmap](#) or null. |
