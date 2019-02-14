[android-components](../../../index.md) / [mozilla.components.concept.fetch](../../index.md) / [Response](../index.md) / [Body](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`Body(stream: `[`InputStream`](https://developer.android.com/reference/java/io/InputStream.html)`, contentType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`

A [Body](index.md) returned along with the [Request](../../-request/index.md).

**The response body can be consumed only once.**.

### Parameters

`stream` - the input stream from which the response body can be read.

`contentType` - optional content-type as provided in the response
header. If specified, an attempt will be made to look up the charset
which will be used for decoding the body. If not specified, or if the
charset can't be found, UTF-8 will be used for decoding.