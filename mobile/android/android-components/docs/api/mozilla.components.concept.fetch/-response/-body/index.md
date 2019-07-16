[android-components](../../../index.md) / [mozilla.components.concept.fetch](../../index.md) / [Response](../index.md) / [Body](./index.md)

# Body

`class Body : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html)`, `[`AutoCloseable`](https://developer.android.com/reference/java/lang/AutoCloseable.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Response.kt#L57)

A [Body](./index.md) returned along with the [Request](../../-request/index.md).

**The response body can be consumed only once.**.

### Parameters

`stream` - the input stream from which the response body can be read.

`contentType` - optional content-type as provided in the response
header. If specified, an attempt will be made to look up the charset
which will be used for decoding the body. If not specified, or if the
charset can't be found, UTF-8 will be used for decoding.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Body(stream: `[`InputStream`](https://developer.android.com/reference/java/io/InputStream.html)`, contentType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>A [Body](./index.md) returned along with the [Request](../../-request/index.md). |

### Functions

| Name | Summary |
|---|---|
| [close](close.md) | `open fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Closes this [Body](./index.md) and releases any system resources associated with it. |
| [string](string.md) | `fun string(charset: `[`Charset`](https://developer.android.com/reference/java/nio/charset/Charset.html)`? = null): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Reads this body completely as a String. |
| [useBufferedReader](use-buffered-reader.md) | `fun <R> useBufferedReader(charset: `[`Charset`](https://developer.android.com/reference/java/nio/charset/Charset.html)`? = null, block: (`[`BufferedReader`](https://developer.android.com/reference/java/io/BufferedReader.html)`) -> `[`R`](use-buffered-reader.md#R)`): `[`R`](use-buffered-reader.md#R)<br>Creates a buffered reader from this body. |
| [useStream](use-stream.md) | `fun <R> useStream(block: (`[`InputStream`](https://developer.android.com/reference/java/io/InputStream.html)`) -> `[`R`](use-stream.md#R)`): `[`R`](use-stream.md#R)<br>Creates a usable stream from this body. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [empty](empty.md) | `fun empty(): `[`Body`](./index.md)<br>Creates an empty response body. |
