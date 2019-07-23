[android-components](../index.md) / [mozilla.components.concept.fetch](./index.md)

## Package mozilla.components.concept.fetch

### Types

| Name | Summary |
|---|---|
| [Client](-client/index.md) | `abstract class Client`<br>A generic [Client](-client/index.md) for fetching resources via HTTP/s. |
| [Header](-header/index.md) | `data class Header`<br>Represents a [Header](-header/index.md) containing of a [name](-header/name.md) and [value](-header/value.md). |
| [Headers](-headers/index.md) | `interface Headers : `[`Iterable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-iterable/index.html)`<`[`Header`](-header/index.md)`>`<br>A collection of HTTP [Headers](-headers/index.md) (immutable) of a [Request](-request/index.md) or [Response](-response/index.md). |
| [MutableHeaders](-mutable-headers/index.md) | `class MutableHeaders : `[`Headers`](-headers/index.md)`, `[`MutableIterable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-iterable/index.html)`<`[`Header`](-header/index.md)`>`<br>A collection of HTTP [Headers](-headers/index.md) (mutable) of a [Request](-request/index.md) or [Response](-response/index.md). |
| [Request](-request/index.md) | `data class Request`<br>The [Request](-request/index.md) data class represents a resource request to be send by a [Client](-client/index.md). |
| [Response](-response/index.md) | `data class Response : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html)<br>The [Response](-response/index.md) data class represents a response to a [Request](-request/index.md) send by a [Client](-client/index.md). |

### Extensions for External Classes

| Name | Summary |
|---|---|
| [kotlin.collections.List](kotlin.collections.-list/index.md) |  |

### Properties

| Name | Summary |
|---|---|
| [isClientError](is-client-error.md) | `val `[`Response`](-response/index.md)`.isClientError: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if the response was a client error (status in the range 400-499) or false otherwise. |
| [isSuccess](is-success.md) | `val `[`Response`](-response/index.md)`.isSuccess: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if the response was successful (status in the range 200-299) or false otherwise. |
