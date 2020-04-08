[android-components](../../index.md) / [mozilla.components.concept.fetch](../index.md) / [Client](index.md) / [fetch](./fetch.md)

# fetch

`abstract fun fetch(request: `[`Request`](../-request/index.md)`): `[`Response`](../-response/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Client.kt#L47)

Starts the process of fetching a resource from the network as described by the [Request](../-request/index.md) object. This call is
synchronous.

A [Response](../-response/index.md) may keep references to open streams. Therefore it's important to always close the [Response](../-response/index.md) or
its [Response.Body](../-response/-body/index.md).

Use the `use()` extension method when performing multiple operations on the [Response](../-response/index.md) object:

``` Kotlin
client.fetch(request).use { response ->
    // Use response. Resources will get released automatically at the end of the block.
}
```

Alternatively you can use multiple `use*()` methods on the [Response.Body](../-response/-body/index.md) object.

### Parameters

`request` - The request to be executed by this [Client](index.md).

### Exceptions

`IOException` - if the request could not be executed due to cancellation, a connectivity problem or a
timeout.

**Return**
The [Response](../-response/index.md) returned by the server.

