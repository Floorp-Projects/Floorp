[android-components](../../../index.md) / [mozilla.components.concept.fetch](../../index.md) / [Request](../index.md) / [Body](./index.md)

# Body

`class Body : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Request.kt#L48)

A [Body](./index.md) to be send with the [Request](../index.md).

### Parameters

`stream` - A stream that will be read and send to the resource.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Body(stream: `[`InputStream`](https://developer.android.com/reference/java/io/InputStream.html)`)`<br>A [Body](./index.md) to be send with the [Request](../index.md). |

### Functions

| Name | Summary |
|---|---|
| [close](close.md) | `fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Closes this body and releases any system resources associated with it. |
| [useStream](use-stream.md) | `fun <R> useStream(block: (`[`InputStream`](https://developer.android.com/reference/java/io/InputStream.html)`) -> `[`R`](use-stream.md#R)`): `[`R`](use-stream.md#R)<br>Executes the given [block](use-stream.md#mozilla.components.concept.fetch.Request.Body$useStream(kotlin.Function1((java.io.InputStream, mozilla.components.concept.fetch.Request.Body.useStream.R)))/block) function on the body's stream and then closes it down correctly whether an exception is thrown or not. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [fromFile](from-file.md) | `fun fromFile(file: `[`File`](https://developer.android.com/reference/java/io/File.html)`): `[`Body`](./index.md)<br>Create a [Body](./index.md) from the provided [File](https://developer.android.com/reference/java/io/File.html). |
| [fromString](from-string.md) | `fun fromString(value: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Body`](./index.md)<br>Create a [Body](./index.md) from the provided [String](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html). |
