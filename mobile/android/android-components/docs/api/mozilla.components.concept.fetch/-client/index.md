[android-components](../../index.md) / [mozilla.components.concept.fetch](../index.md) / [Client](./index.md)

# Client

`abstract class Client` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Client.kt#L23)

A generic [Client](./index.md) for fetching resources via HTTP/s.

Abstract base class / interface for clients implementing the `concept-fetch` component.

The [Request](../-request/index.md)/[Response](../-response/index.md) API is inspired by the Web Fetch API:
https://developer.mozilla.org/en-US/docs/Web/API/Fetch_API

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Client()`<br>A generic [Client](./index.md) for fetching resources via HTTP/s. |

### Properties

| Name | Summary |
|---|---|
| [defaultHeaders](default-headers.md) | `val defaultHeaders: `[`Headers`](../-headers/index.md)<br>List of default headers that should be added to every request unless overridden by the headers in the request. |

### Functions

| Name | Summary |
|---|---|
| [fetch](fetch.md) | `abstract fun fetch(request: `[`Request`](../-request/index.md)`): `[`Response`](../-response/index.md)<br>Starts the process of fetching a resource from the network as described by the [Request](../-request/index.md) object. This call is synchronous. |
| [fetchDataUri](fetch-data-uri.md) | `fun fetchDataUri(request: `[`Request`](../-request/index.md)`): `[`Response`](../-response/index.md)<br>Generates a [Response](../-response/index.md) based on the provided [Request](../-request/index.md) for a data URI. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [DATA_URI_BASE64_EXT](-d-a-t-a_-u-r-i_-b-a-s-e64_-e-x-t.md) | `const val DATA_URI_BASE64_EXT: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [DATA_URI_CHARSET](-d-a-t-a_-u-r-i_-c-h-a-r-s-e-t.md) | `const val DATA_URI_CHARSET: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [DATA_URI_SCHEME](-d-a-t-a_-u-r-i_-s-c-h-e-m-e.md) | `const val DATA_URI_SCHEME: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
| [withInterceptors](../../mozilla.components.concept.fetch.interceptor/with-interceptors.md) | `fun `[`Client`](./index.md)`.withInterceptors(vararg interceptors: `[`Interceptor`](../../mozilla.components.concept.fetch.interceptor/-interceptor/index.md)`): `[`Client`](./index.md)<br>Creates a new [Client](./index.md) instance that will use the provided list of [Interceptor](../../mozilla.components.concept.fetch.interceptor/-interceptor/index.md) instances. |

### Inheritors

| Name | Summary |
|---|---|
| [GeckoViewFetchClient](../../mozilla.components.browser.engine.gecko.fetch/-gecko-view-fetch-client/index.md) | `class GeckoViewFetchClient : `[`Client`](./index.md)<br>GeckoView ([GeckoWebExecutor](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoWebExecutor.html)) based implementation of [Client](./index.md). |
| [HttpURLConnectionClient](../../mozilla.components.lib.fetch.httpurlconnection/-http-u-r-l-connection-client/index.md) | `class HttpURLConnectionClient : `[`Client`](./index.md)<br>[HttpURLConnection](http://docs.oracle.com/javase/7/docs/api/java/net/HttpURLConnection.html) implementation of [Client](./index.md). |
| [OkHttpClient](../../mozilla.components.lib.fetch.okhttp/-ok-http-client/index.md) | `class OkHttpClient : `[`Client`](./index.md)<br>[Client](./index.md) implementation using OkHttp. |
