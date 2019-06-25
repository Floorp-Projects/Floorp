[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.fetch](../index.md) / [GeckoViewFetchClient](./index.md)

# GeckoViewFetchClient

`class GeckoViewFetchClient : `[`Client`](../../mozilla.components.concept.fetch/-client/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/fetch/GeckoViewFetchClient.kt#L31)

GeckoView ([GeckoWebExecutor](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoWebExecutor.html)) based implementation of [Client](../../mozilla.components.concept.fetch/-client/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GeckoViewFetchClient(context: <ERROR CLASS>, runtime: `[`GeckoRuntime`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoRuntime.html)` = GeckoRuntime.getDefault(context), maxReadTimeOut: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`TimeUnit`](https://developer.android.com/reference/java/util/concurrent/TimeUnit.html)`> = Pair(MAX_READ_TIMEOUT_MINUTES, TimeUnit.MINUTES))`<br>GeckoView ([GeckoWebExecutor](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoWebExecutor.html)) based implementation of [Client](../../mozilla.components.concept.fetch/-client/index.md). |

### Inherited Properties

| Name | Summary |
|---|---|
| [defaultHeaders](../../mozilla.components.concept.fetch/-client/default-headers.md) | `val defaultHeaders: `[`Headers`](../../mozilla.components.concept.fetch/-headers/index.md)<br>List of default headers that should be added to every request unless overridden by the headers in the request. |

### Functions

| Name | Summary |
|---|---|
| [fetch](fetch.md) | `fun fetch(request: `[`Request`](../../mozilla.components.concept.fetch/-request/index.md)`): `[`Response`](../../mozilla.components.concept.fetch/-response/index.md)<br>Starts the process of fetching a resource from the network as described by the [Request](../../mozilla.components.concept.fetch/-request/index.md) object. This call is synchronous. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [MAX_READ_TIMEOUT_MINUTES](-m-a-x_-r-e-a-d_-t-i-m-e-o-u-t_-m-i-n-u-t-e-s.md) | `const val MAX_READ_TIMEOUT_MINUTES: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [withInterceptors](../../mozilla.components.concept.fetch.interceptor/with-interceptors.md) | `fun `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`.withInterceptors(vararg interceptors: `[`Interceptor`](../../mozilla.components.concept.fetch.interceptor/-interceptor/index.md)`): `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)<br>Creates a new [Client](../../mozilla.components.concept.fetch/-client/index.md) instance that will use the provided list of [Interceptor](../../mozilla.components.concept.fetch.interceptor/-interceptor/index.md) instances. |
