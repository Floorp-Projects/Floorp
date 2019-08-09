[android-components](../../index.md) / [mozilla.components.service.glean.config](../index.md) / [Configuration](./index.md)

# Configuration

`data class Configuration` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/config/Configuration.kt#L29)

The Configuration class describes how to configure the Glean.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Configuration(serverEndpoint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = DEFAULT_TELEMETRY_ENDPOINT, connectionTimeout: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = DEFAULT_CONNECTION_TIMEOUT, readTimeout: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = DEFAULT_READ_TIMEOUT, maxEvents: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = DEFAULT_MAX_EVENTS, httpClient: `[`Lazy`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-lazy/index.html)`<`[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`> = lazy { HttpURLConnectionClient() }, channel: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)` |

### Properties

| Name | Summary |
|---|---|
| [channel](channel.md) | `val channel: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>the release channel the application is on, if known. This will be     sent along with all the pings, in the `client_info` section. |
| [connectionTimeout](connection-timeout.md) | `val connectionTimeout: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>the timeout, in milliseconds, to use when connecting to     the [serverEndpoint](server-endpoint.md) |
| [httpClient](http-client.md) | `val httpClient: `[`Lazy`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-lazy/index.html)`<`[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`>`<br>The HTTP client implementation to use for uploading pings. |
| [logPings](log-pings.md) | `val logPings: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>whether to log ping contents to the console. This is only meant to be used     internally by the `GleanDebugActivity`. |
| [maxEvents](max-events.md) | `val maxEvents: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>the number of events to store before the events ping is sent |
| [pingTag](ping-tag.md) | `val pingTag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>String tag to be applied to headers when uploading pings for debug view.     This is only meant to be used internally by the `GleanDebugActivity`. |
| [readTimeout](read-timeout.md) | `val readTimeout: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>the timeout, in milliseconds, to use when connecting to     the [serverEndpoint](server-endpoint.md) |
| [serverEndpoint](server-endpoint.md) | `val serverEndpoint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the server pings are sent to. Please note that this is     is only meant to be changed for tests. |
| [userAgent](user-agent.md) | `val userAgent: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the user agent used when sending pings, only to be used internally. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [DEFAULT_CONNECTION_TIMEOUT](-d-e-f-a-u-l-t_-c-o-n-n-e-c-t-i-o-n_-t-i-m-e-o-u-t.md) | `const val DEFAULT_CONNECTION_TIMEOUT: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |
| [DEFAULT_LOG_PINGS](-d-e-f-a-u-l-t_-l-o-g_-p-i-n-g-s.md) | `const val DEFAULT_LOG_PINGS: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [DEFAULT_MAX_EVENTS](-d-e-f-a-u-l-t_-m-a-x_-e-v-e-n-t-s.md) | `const val DEFAULT_MAX_EVENTS: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [DEFAULT_READ_TIMEOUT](-d-e-f-a-u-l-t_-r-e-a-d_-t-i-m-e-o-u-t.md) | `const val DEFAULT_READ_TIMEOUT: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |
| [DEFAULT_TELEMETRY_ENDPOINT](-d-e-f-a-u-l-t_-t-e-l-e-m-e-t-r-y_-e-n-d-p-o-i-n-t.md) | `const val DEFAULT_TELEMETRY_ENDPOINT: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [DEFAULT_USER_AGENT](-d-e-f-a-u-l-t_-u-s-e-r_-a-g-e-n-t.md) | `const val DEFAULT_USER_AGENT: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
