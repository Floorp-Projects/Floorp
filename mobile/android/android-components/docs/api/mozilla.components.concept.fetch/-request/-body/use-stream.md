[android-components](../../../index.md) / [mozilla.components.concept.fetch](../../index.md) / [Request](../index.md) / [Body](index.md) / [useStream](./use-stream.md)

# useStream

`fun <R> useStream(block: (`[`InputStream`](http://docs.oracle.com/javase/7/docs/api/java/io/InputStream.html)`) -> `[`R`](use-stream.md#R)`): `[`R`](use-stream.md#R) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Request.kt#L92)

Executes the given [block](use-stream.md#mozilla.components.concept.fetch.Request.Body$useStream(kotlin.Function1((java.io.InputStream, mozilla.components.concept.fetch.Request.Body.useStream.R)))/block) function on the body's stream and then closes it down correctly whether an
exception is thrown or not.

