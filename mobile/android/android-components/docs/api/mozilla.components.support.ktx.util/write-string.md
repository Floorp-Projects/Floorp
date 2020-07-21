[android-components](../index.md) / [mozilla.components.support.ktx.util](index.md) / [writeString](./write-string.md)

# writeString

`inline fun <ERROR CLASS>.writeString(block: () -> `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/util/AtomicFile.kt#L36)

Writes an [AtomicFile](#) and indicates if the file was wrote.

### Parameters

`block` - A function with provides the content of the file as a [String](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)

**Return**
true if the file wrote otherwise false

