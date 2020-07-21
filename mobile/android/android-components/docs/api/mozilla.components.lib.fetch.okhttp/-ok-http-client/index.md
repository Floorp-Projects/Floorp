[android-components](../../index.md) / [mozilla.components.lib.fetch.okhttp](../index.md) / [OkHttpClient](./index.md)

# OkHttpClient

`class OkHttpClient : `[`Client`](../../mozilla.components.concept.fetch/-client/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/fetch-okhttp/src/main/java/mozilla/components/lib/fetch/okhttp/OkHttpClient.kt#L29)

[Client](../../mozilla.components.concept.fetch/-client/index.md) implementation using OkHttp.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `OkHttpClient(client: OkHttpClient = OkHttpClient(), context: <ERROR CLASS>? = null)`<br>[Client](../../mozilla.components.concept.fetch/-client/index.md) implementation using OkHttp. |

### Inherited Properties

| Name | Summary |
|---|---|
| [defaultHeaders](../../mozilla.components.concept.fetch/-client/default-headers.md) | `val defaultHeaders: `[`Headers`](../../mozilla.components.concept.fetch/-headers/index.md)<br>List of default headers that should be added to every request unless overridden by the headers in the request. |

### Functions

| Name | Summary |
|---|---|
| [fetch](fetch.md) | `fun fetch(request: `[`Request`](../../mozilla.components.concept.fetch/-request/index.md)`): `[`Response`](../../mozilla.components.concept.fetch/-response/index.md)<br>Starts the process of fetching a resource from the network as described by the [Request](../../mozilla.components.concept.fetch/-request/index.md) object. This call is synchronous. |

### Inherited Functions

| Name | Summary |
|---|---|
| [fetchDataUri](../../mozilla.components.concept.fetch/-client/fetch-data-uri.md) | `fun fetchDataUri(request: `[`Request`](../../mozilla.components.concept.fetch/-request/index.md)`): `[`Response`](../../mozilla.components.concept.fetch/-response/index.md)<br>Generates a [Response](../../mozilla.components.concept.fetch/-response/index.md) based on the provided [Request](../../mozilla.components.concept.fetch/-request/index.md) for a data URI. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [getOrCreateCookieManager](get-or-create-cookie-manager.md) | `fun getOrCreateCookieManager(): `[`CookieManager`](http://docs.oracle.com/javase/7/docs/api/java/net/CookieManager.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
| [withInterceptors](../../mozilla.components.concept.fetch.interceptor/with-interceptors.md) | `fun `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`.withInterceptors(vararg interceptors: `[`Interceptor`](../../mozilla.components.concept.fetch.interceptor/-interceptor/index.md)`): `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)<br>Creates a new [Client](../../mozilla.components.concept.fetch/-client/index.md) instance that will use the provided list of [Interceptor](../../mozilla.components.concept.fetch.interceptor/-interceptor/index.md) instances. |
