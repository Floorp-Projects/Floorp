[android-components](../../index.md) / [mozilla.components.service.experiments](../index.md) / [Configuration](./index.md)

# Configuration

`data class Configuration` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/Configuration.kt#L17)

The Configuration class describes how to configure Experiments.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Configuration(httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, kintoEndpoint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = ExperimentsUpdater.KINTO_ENDPOINT_PROD)`<br>The Configuration class describes how to configure Experiments. |

### Properties

| Name | Summary |
|---|---|
| [httpClient](http-client.md) | `val httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)<br>The HTTP client implementation to use for uploading pings. |
| [kintoEndpoint](kinto-endpoint.md) | `val kintoEndpoint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the endpoint to fetch experiments from, must be one of: [ExperimentsUpdater.KINTO_ENDPOINT_DEV](#), [ExperimentsUpdater.KINTO_ENDPOINT_STAGING](#), or [ExperimentsUpdater.KINTO_ENDPOINT_PROD](#) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
