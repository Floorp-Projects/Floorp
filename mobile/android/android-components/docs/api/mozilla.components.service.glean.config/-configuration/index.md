[android-components](../../index.md) / [mozilla.components.service.glean.config](../index.md) / [Configuration](./index.md)

# Configuration

`data class Configuration` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/config/Configuration.kt#L22)

The Configuration class describes how to configure the Glean.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Configuration(serverEndpoint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = GleanCoreConfiguration.DEFAULT_TELEMETRY_ENDPOINT, channel: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, maxEvents: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null, httpClient: PingUploader = ConceptFetchHttpUploader(lazy { HttpURLConnectionClient() }))`<br>The Configuration class describes how to configure the Glean. |

### Properties

| Name | Summary |
|---|---|
| [channel](channel.md) | `val channel: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>the release channel the application is on, if known. This will be     sent along with all the pings, in the `client_info` section. |
| [httpClient](http-client.md) | `val httpClient: PingUploader`<br>The HTTP client implementation to use for uploading pings. |
| [maxEvents](max-events.md) | `val maxEvents: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?`<br>the number of events to store before the events ping is sent |
| [serverEndpoint](server-endpoint.md) | `val serverEndpoint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the server pings are sent to. Please note that this is     is only meant to be changed for tests. |

### Functions

| Name | Summary |
|---|---|
| [toWrappedConfiguration](to-wrapped-configuration.md) | `fun toWrappedConfiguration(): Configuration`<br>Convert the Android Components configuration object to the Glean SDK configuration object. |
