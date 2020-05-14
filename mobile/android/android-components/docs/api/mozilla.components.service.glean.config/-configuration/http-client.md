[android-components](../../index.md) / [mozilla.components.service.glean.config](../index.md) / [Configuration](index.md) / [httpClient](./http-client.md)

# httpClient

`val httpClient: PingUploader` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/config/Configuration.kt#L24)

The HTTP client implementation to use for uploading pings.
    If you don't provide your own networking stack with an HTTP client to use,
    you can fall back to a simple implementation on top of `java.net` using
    `ConceptFetchHttpUploader(lazy { HttpURLConnectionClient() as Client })`

### Property

`httpClient` - The HTTP client implementation to use for uploading pings.
    If you don't provide your own networking stack with an HTTP client to use,
    you can fall back to a simple implementation on top of `java.net` using
    `ConceptFetchHttpUploader(lazy { HttpURLConnectionClient() as Client })`