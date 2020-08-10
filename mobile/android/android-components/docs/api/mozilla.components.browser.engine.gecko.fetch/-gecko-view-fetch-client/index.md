[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.fetch](../index.md) / [GeckoViewFetchClient](./index.md)

# GeckoViewFetchClient

`class GeckoViewFetchClient : `[`Client`](../../mozilla.components.concept.fetch/-client/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/fetch/GeckoViewFetchClient.kt#L34)

GeckoView ([GeckoWebExecutor](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoWebExecutor.html)) based implementation of [Client](../../mozilla.components.concept.fetch/-client/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GeckoViewFetchClient(context: <ERROR CLASS>, runtime: `[`GeckoRuntime`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoRuntime.html)` = GeckoRuntime.getDefault(context), maxReadTimeOut: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`TimeUnit`](http://docs.oracle.com/javase/7/docs/api/java/util/concurrent/TimeUnit.html)`> = Pair(MAX_READ_TIMEOUT_MINUTES, TimeUnit.MINUTES))`<br>GeckoView ([GeckoWebExecutor](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoWebExecutor.html)) based implementation of [Client](../../mozilla.components.concept.fetch/-client/index.md). |

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

### Companion Object Properties

| Name | Summary |
|---|---|
| [MAX_READ_TIMEOUT_MINUTES](-m-a-x_-r-e-a-d_-t-i-m-e-o-u-t_-m-i-n-u-t-e-s.md) | `const val MAX_READ_TIMEOUT_MINUTES: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
| [withInterceptors](../../mozilla.components.concept.fetch.interceptor/with-interceptors.md) | `fun `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`.withInterceptors(vararg interceptors: `[`Interceptor`](../../mozilla.components.concept.fetch.interceptor/-interceptor/index.md)`): `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)<br>Creates a new [Client](../../mozilla.components.concept.fetch/-client/index.md) instance that will use the provided list of [Interceptor](../../mozilla.components.concept.fetch.interceptor/-interceptor/index.md) instances. |
