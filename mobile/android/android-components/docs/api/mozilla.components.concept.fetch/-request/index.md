[android-components](../../index.md) / [mozilla.components.concept.fetch](../index.md) / [Request](./index.md)

# Request

`data class Request` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Request.kt#L40)

The [Request](./index.md) data class represents a resource request to be send by a [Client](../-client/index.md).

It's API is inspired by the Request interface of the Web Fetch API:
https://developer.mozilla.org/en-US/docs/Web/API/Request

### Types

| Name | Summary |
|---|---|
| [Body](-body/index.md) | `class Body : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html)<br>A [Body](-body/index.md) to be send with the [Request](./index.md). |
| [CookiePolicy](-cookie-policy/index.md) | `enum class CookiePolicy` |
| [Method](-method/index.md) | `enum class Method`<br>Request methods. |
| [Redirect](-redirect/index.md) | `enum class Redirect` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Request(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, method: `[`Method`](-method/index.md)` = Method.GET, headers: `[`MutableHeaders`](../-mutable-headers/index.md)`? = MutableHeaders(), connectTimeout: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`TimeUnit`](https://developer.android.com/reference/java/util/concurrent/TimeUnit.html)`>? = null, readTimeout: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`TimeUnit`](https://developer.android.com/reference/java/util/concurrent/TimeUnit.html)`>? = null, body: `[`Body`](-body/index.md)`? = null, redirect: `[`Redirect`](-redirect/index.md)` = Redirect.FOLLOW, cookiePolicy: `[`CookiePolicy`](-cookie-policy/index.md)` = CookiePolicy.INCLUDE, useCaches: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)`<br>The [Request](./index.md) data class represents a resource request to be send by a [Client](../-client/index.md). |

### Properties

| Name | Summary |
|---|---|
| [body](body.md) | `val body: `[`Body`](-body/index.md)`?`<br>An optional body to be send with the request. |
| [connectTimeout](connect-timeout.md) | `val connectTimeout: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`TimeUnit`](https://developer.android.com/reference/java/util/concurrent/TimeUnit.html)`>?`<br>A timeout to be used when connecting to the resource.  If the timeout expires before the connection can be established, a [java.net.SocketTimeoutException](https://developer.android.com/reference/java/net/SocketTimeoutException.html) is raised. A timeout of zero is interpreted as an infinite timeout. |
| [cookiePolicy](cookie-policy.md) | `val cookiePolicy: `[`CookiePolicy`](-cookie-policy/index.md)<br>A policy to specify whether or not cookies should be sent with the request, defaults to [CookiePolicy.INCLUDE](-cookie-policy/-i-n-c-l-u-d-e.md) |
| [headers](headers.md) | `val headers: `[`MutableHeaders`](../-mutable-headers/index.md)`?`<br>Optional HTTP headers to be send with the request. |
| [method](method.md) | `val method: `[`Method`](-method/index.md)<br>The request method (GET, POST, ..) |
| [readTimeout](read-timeout.md) | `val readTimeout: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`TimeUnit`](https://developer.android.com/reference/java/util/concurrent/TimeUnit.html)`>?`<br>A timeout to be used when reading from the resource. If the timeout expires before there is data available for read, a java.net.SocketTimeoutException is raised. A timeout of zero is interpreted as an infinite timeout. |
| [redirect](redirect.md) | `val redirect: `[`Redirect`](-redirect/index.md)<br>Whether the [Client](../-client/index.md) should follow redirects (HTTP 3xx) for this request or not. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The URL of the request. |
| [useCaches](use-caches.md) | `val useCaches: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Whether caches should be used or a network request should be forced, defaults to true (use caches). |
