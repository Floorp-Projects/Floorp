[android-components](../index.md) / [mozilla.components.support.ktx.util](index.md) / [readAndDeserialize](./read-and-deserialize.md)

# readAndDeserialize

`inline fun <T> <ERROR CLASS>.readAndDeserialize(block: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`T`](read-and-deserialize.md#T)`): `[`T`](read-and-deserialize.md#T)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/util/AtomicFile.kt#L18)

Reads an [AtomicFile](#) and provides a deserialized version of its content.

### Parameters

`block` - A function to be executed after the file is read and provides the content as
a [String](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html). It is expected that this function returns a deserialized version of the content
of the file.