[android-components](../../index.md) / [mozilla.components.service.glean.config](../index.md) / [Configuration](./index.md)

# Configuration

`data class Configuration` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/config/Configuration.kt#L24)

The Configuration class describes how to configure the Glean.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Configuration(serverEndpoint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "https://incoming.telemetry.mozilla.org", userAgent: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "Glean/${BuildConfig.LIBRARY_VERSION} (Android)", connectionTimeout: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 10000, readTimeout: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 30000, maxEvents: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 500, logPings: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)` = HttpURLConnectionClient())`<br>The Configuration class describes how to configure the Glean. |

### Properties

| Name | Summary |
|---|---|
| [connectionTimeout](connection-timeout.md) | `val connectionTimeout: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>the timeout, in milliseconds, to use when connecting to     the [serverEndpoint](server-endpoint.md) |
| [httpClient](http-client.md) | `val httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)<br>The HTTP client implementation to use for uploading pings. |
| [logPings](log-pings.md) | `val logPings: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>whether to log ping contents to the console. |
| [maxEvents](max-events.md) | `val maxEvents: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>the number of events to store before the events ping is sent |
| [readTimeout](read-timeout.md) | `val readTimeout: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>the timeout, in milliseconds, to use when connecting to     the [serverEndpoint](server-endpoint.md) |
| [serverEndpoint](server-endpoint.md) | `val serverEndpoint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the server pings are sent to |
| [userAgent](user-agent.md) | `val userAgent: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the user agent used when sending pings |
