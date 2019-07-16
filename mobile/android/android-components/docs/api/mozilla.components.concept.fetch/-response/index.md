[android-components](../../index.md) / [mozilla.components.concept.fetch](../index.md) / [Response](./index.md)

# Response

`data class Response : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Response.kt#L33)

The [Response](./index.md) data class represents a response to a [Request](../-request/index.md) send by a [Client](../-client/index.md).

You can create a [Response](./index.md) object using the constructor, but you are more likely to encounter a [Response](./index.md) object
being returned as the result of calling [Client.fetch](../-client/fetch.md).

A [Response](./index.md) may hold references to other resources (e.g. streams). Therefore it's important to always close the
[Response](./index.md) object or its [Body](-body/index.md). This can be done by either consuming the content of the [Body](-body/index.md) with one of the
available methods or by using Kotlin's extension methods for using [Closeable](https://developer.android.com/reference/java/io/Closeable.html) implementations (like `use()`):

``` Kotlin
val response = ...
response.use {
   // Use response. Resources will get released automatically at the end of the block.
}
```

### Types

| Name | Summary |
|---|---|
| [Body](-body/index.md) | `class Body : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html)`, `[`AutoCloseable`](https://developer.android.com/reference/java/lang/AutoCloseable.html)<br>A [Body](-body/index.md) returned along with the [Request](../-request/index.md). |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Response(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, status: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, headers: `[`Headers`](../-headers/index.md)`, body: `[`Body`](-body/index.md)`)`<br>The [Response](./index.md) data class represents a response to a [Request](../-request/index.md) send by a [Client](../-client/index.md). |

### Properties

| Name | Summary |
|---|---|
| [body](body.md) | `val body: `[`Body`](-body/index.md) |
| [headers](headers.md) | `val headers: `[`Headers`](../-headers/index.md) |
| [status](status.md) | `val status: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Functions

| Name | Summary |
|---|---|
| [close](close.md) | `fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Closes this [Response](./index.md) and its [Body](-body/index.md) and releases any system resources associated with it. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [CLIENT_ERROR_STATUS_RANGE](-c-l-i-e-n-t_-e-r-r-o-r_-s-t-a-t-u-s_-r-a-n-g-e.md) | `val CLIENT_ERROR_STATUS_RANGE: `[`IntRange`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.ranges/-int-range/index.html) |
| [SUCCESS_STATUS_RANGE](-s-u-c-c-e-s-s_-s-t-a-t-u-s_-r-a-n-g-e.md) | `val SUCCESS_STATUS_RANGE: `[`IntRange`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.ranges/-int-range/index.html) |

### Extension Properties

| Name | Summary |
|---|---|
| [isClientError](../is-client-error.md) | `val `[`Response`](./index.md)`.isClientError: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if the response was a client error (status in the range 400-499) or false otherwise. |
| [isSuccess](../is-success.md) | `val `[`Response`](./index.md)`.isSuccess: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if the response was successful (status in the range 200-299) or false otherwise. |
