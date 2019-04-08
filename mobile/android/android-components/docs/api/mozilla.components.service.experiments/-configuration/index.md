[android-components](../../index.md) / [mozilla.components.service.experiments](../index.md) / [Configuration](./index.md)

# Configuration

`data class Configuration` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/Configuration.kt#L15)

The Configuration class describes how to configure Experiments.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Configuration(httpClient: `[`Lazy`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-lazy/index.html)`<`[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`> = lazy { HttpURLConnectionClient() })`<br>The Configuration class describes how to configure Experiments. |

### Properties

| Name | Summary |
|---|---|
| [httpClient](http-client.md) | `val httpClient: `[`Lazy`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-lazy/index.html)`<`[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`>`<br>The HTTP client implementation to use for uploading pings. |
