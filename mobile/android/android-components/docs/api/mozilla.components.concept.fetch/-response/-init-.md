[android-components](../../index.md) / [mozilla.components.concept.fetch](../index.md) / [Response](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`Response(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, status: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, headers: `[`Headers`](../-headers/index.md)`, body: `[`Body`](-body/index.md)`)`

The [Response](index.md) data class represents a response to a [Request](../-request/index.md) send by a [Client](../-client/index.md).

You can create a [Response](index.md) object using the constructor, but you are more likely to encounter a [Response](index.md) object
being returned as the result of calling [Client.fetch](../-client/fetch.md).

A [Response](index.md) may hold references to other resources (e.g. streams). Therefore it's important to always close the
[Response](index.md) object or its [Body](-body/index.md). This can be done by either consuming the content of the [Body](-body/index.md) with one of the
available methods or by using Kotlin's extension methods for using [Closeable](http://docs.oracle.com/javase/7/docs/api/java/io/Closeable.html) implementations (like `use()`):

``` Kotlin
val response = ...
response.use {
   // Use response. Resources will get released automatically at the end of the block.
}
```

