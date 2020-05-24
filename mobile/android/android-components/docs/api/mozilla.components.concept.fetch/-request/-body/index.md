[android-components](../../../index.md) / [mozilla.components.concept.fetch](../../index.md) / [Request](../index.md) / [Body](./index.md)

# Body

`class Body : `[`Closeable`](http://docs.oracle.com/javase/7/docs/api/java/io/Closeable.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Request.kt#L56)

A [Body](./index.md) to be send with the [Request](../index.md).

### Parameters

`stream` - A stream that will be read and send to the resource.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Body(stream: `[`InputStream`](http://docs.oracle.com/javase/7/docs/api/java/io/InputStream.html)`)`<br>A [Body](./index.md) to be send with the [Request](../index.md). |

### Functions

| Name | Summary |
|---|---|
| [close](close.md) | `fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Closes this body and releases any system resources associated with it. |
| [useStream](use-stream.md) | `fun <R> useStream(block: (`[`InputStream`](http://docs.oracle.com/javase/7/docs/api/java/io/InputStream.html)`) -> `[`R`](use-stream.md#R)`): `[`R`](use-stream.md#R)<br>Executes the given [block](use-stream.md#mozilla.components.concept.fetch.Request.Body$useStream(kotlin.Function1((java.io.InputStream, mozilla.components.concept.fetch.Request.Body.useStream.R)))/block) function on the body's stream and then closes it down correctly whether an exception is thrown or not. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [fromFile](from-file.md) | `fun fromFile(file: `[`File`](http://docs.oracle.com/javase/7/docs/api/java/io/File.html)`): `[`Body`](./index.md)<br>Create a [Body](./index.md) from the provided [File](http://docs.oracle.com/javase/7/docs/api/java/io/File.html). |
| [fromParamsForFormUrlEncoded](from-params-for-form-url-encoded.md) | `fun fromParamsForFormUrlEncoded(vararg unencodedParams: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`Body`](./index.md)<br>Create a [Body](./index.md) from the provided [unencodedParams](from-params-for-form-url-encoded.md#mozilla.components.concept.fetch.Request.Body.Companion$fromParamsForFormUrlEncoded(kotlin.Array((kotlin.Pair((kotlin.String, )))))/unencodedParams) in the format of Content-Type "application/x-www-form-urlencoded". Parameters are formatted as "key1=value1&key2=value2..." and values are percent-encoded. If the given map is empty, the response body will contain the empty string. |
| [fromString](from-string.md) | `fun fromString(value: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Body`](./index.md)<br>Create a [Body](./index.md) from the provided [String](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html). |
