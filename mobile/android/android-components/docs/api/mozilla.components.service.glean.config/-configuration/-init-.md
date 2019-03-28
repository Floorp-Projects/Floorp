[android-components](../../index.md) / [mozilla.components.service.glean.config](../index.md) / [Configuration](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`Configuration(serverEndpoint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = DEFAULT_TELEMETRY_ENDPOINT, userAgent: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "Glean/${BuildConfig.LIBRARY_VERSION} (Android)", connectionTimeout: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 10000, readTimeout: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 30000, maxEvents: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 500, logPings: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, httpClient: `[`Lazy`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-lazy/index.html)`<`[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`> = lazy { HttpURLConnectionClient() }, pingTag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, channel: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`

The Configuration class describes how to configure the Glean.

