[android-components](../../../index.md) / [mozilla.components.concept.fetch](../../index.md) / [Response](../index.md) / [Body](index.md) / [useStream](./use-stream.md)

# useStream

`fun <R> useStream(block: (`[`InputStream`](https://developer.android.com/reference/java/io/InputStream.html)`) -> `[`R`](use-stream.md#R)`): `[`R`](use-stream.md#R) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Response.kt#L78)

Creates a usable stream from this body.

Executes the given [block](use-stream.md#mozilla.components.concept.fetch.Response.Body$useStream(kotlin.Function1((java.io.InputStream, mozilla.components.concept.fetch.Response.Body.useStream.R)))/block) function with the stream as parameter and then closes it down correctly
whether an exception is thrown or not.

