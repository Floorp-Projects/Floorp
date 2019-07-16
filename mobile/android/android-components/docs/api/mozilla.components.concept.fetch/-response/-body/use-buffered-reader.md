[android-components](../../../index.md) / [mozilla.components.concept.fetch](../../index.md) / [Response](../index.md) / [Body](index.md) / [useBufferedReader](./use-buffered-reader.md)

# useBufferedReader

`fun <R> useBufferedReader(charset: `[`Charset`](https://developer.android.com/reference/java/nio/charset/Charset.html)`? = null, block: (`[`BufferedReader`](https://developer.android.com/reference/java/io/BufferedReader.html)`) -> `[`R`](use-buffered-reader.md#R)`): `[`R`](use-buffered-reader.md#R) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Response.kt#L94)

Creates a buffered reader from this body.

Executes the given [block](use-buffered-reader.md#mozilla.components.concept.fetch.Response.Body$useBufferedReader(java.nio.charset.Charset, kotlin.Function1((java.io.BufferedReader, mozilla.components.concept.fetch.Response.Body.useBufferedReader.R)))/block) function with the buffered reader as parameter and then closes it down correctly
whether an exception is thrown or not.

### Parameters

`charset` - the optional charset to use when decoding the body. If not specified,
the charset provided in the response content-type header will be used. If the header
is missing or the charset is not supported, UTF-8 will be used.

`block` - a function to consume the buffered reader.