[android-components](../../index.md) / [mozilla.components.lib.fetch.httpurlconnection](../index.md) / [HttpURLConnectionClient](index.md) / [fetch](./fetch.md)

# fetch

`fun fetch(request: `[`Request`](../../mozilla.components.concept.fetch/-request/index.md)`): `[`Response`](../../mozilla.components.concept.fetch/-response/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/fetch-httpurlconnection/src/main/java/mozilla/components/lib/fetch/httpurlconnection/HttpURLConnectionClient.kt#L27)

Overrides [Client.fetch](../../mozilla.components.concept.fetch/-client/fetch.md)

Starts the process of fetching a resource from the network as described by the [Request](../../mozilla.components.concept.fetch/-request/index.md) object. This call is
synchronous.

A [Response](../../mozilla.components.concept.fetch/-response/index.md) may keep references to open streams. Therefore it's important to always close the [Response](../../mozilla.components.concept.fetch/-response/index.md) or
its [Response.Body](../../mozilla.components.concept.fetch/-response/-body/index.md).

Use the `use()` extension method when performing multiple operations on the [Response](../../mozilla.components.concept.fetch/-response/index.md) object:

``` Kotlin
client.fetch(request).use { response ->
    // Use response. Resources will get released automatically at the end of the block.
}
```

Alternatively you can use multiple `use*()` methods on the [Response.Body](../../mozilla.components.concept.fetch/-response/-body/index.md) object.

### Parameters

`request` - The request to be executed by this [Client](../../mozilla.components.concept.fetch/-client/index.md).

### Exceptions

`IOException` - if the request could not be executed due to cancellation, a connectivity problem or a
timeout.

**Return**
The [Response](../../mozilla.components.concept.fetch/-response/index.md) returned by the server.

