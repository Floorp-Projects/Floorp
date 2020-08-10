[android-components](../../index.md) / [mozilla.components.service.glean.net](../index.md) / [ConceptFetchHttpUploader](index.md) / [fromClient](./from-client.md)

# fromClient

`@JvmStatic fun fromClient(client: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`): `[`ConceptFetchHttpUploader`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/net/ConceptFetchHttpUploader.kt#L46)

Export a constructor that is usable from Java.

This looses the lazyness of creating the `client`.

