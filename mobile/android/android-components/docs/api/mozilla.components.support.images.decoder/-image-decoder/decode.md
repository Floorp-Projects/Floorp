[android-components](../../index.md) / [mozilla.components.support.images.decoder](../index.md) / [ImageDecoder](index.md) / [decode](./decode.md)

# decode

`abstract fun decode(data: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`, desiredSize: `[`DesiredSize`](../../mozilla.components.support.images/-desired-size/index.md)`): <ERROR CLASS>?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/images/src/main/java/mozilla/components/support/images/decoder/ImageDecoder.kt#L23)

Decodes the given [data](decode.md#mozilla.components.support.images.decoder.ImageDecoder$decode(kotlin.ByteArray, mozilla.components.support.images.DesiredSize)/data) into a a [Bitmap](#) or null.

The caller provides a maximum size. This is useful for image formats that may decode into multiple images. The
decoder can use this size to determine which [Bitmap](#) to return.

