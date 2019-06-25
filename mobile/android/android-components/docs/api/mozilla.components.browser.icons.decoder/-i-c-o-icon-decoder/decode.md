[android-components](../../index.md) / [mozilla.components.browser.icons.decoder](../index.md) / [ICOIconDecoder](index.md) / [decode](./decode.md)

# decode

`fun decode(data: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`, desiredSize: `[`DesiredSize`](../../mozilla.components.browser.icons/-desired-size/index.md)`): <ERROR CLASS>?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/decoder/ICOIconDecoder.kt#L25)

Overrides [IconDecoder.decode](../-icon-decoder/decode.md)

Decodes the given [data](../-icon-decoder/decode.md#mozilla.components.browser.icons.decoder.IconDecoder$decode(kotlin.ByteArray, mozilla.components.browser.icons.DesiredSize)/data) into a a [Bitmap](#) or null.

The caller provides a maximum size. This is useful for image formats that may decode into multiple images. The
decoder can use this size to determine which [Bitmap](#) to return.

