[android-components](../../index.md) / [mozilla.components.service.pocket](../index.md) / [PocketEndpointRaw](index.md) / [newInstance](./new-instance.md)

# newInstance

`fun newInstance(client: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, pocketApiKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, userAgent: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`PocketEndpointRaw`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/pocket/src/main/java/mozilla/components/service/pocket/PocketEndpointRaw.kt#L57)

Returns a new instance of [PocketEndpointRaw](index.md).

### Parameters

`client` - the HTTP client to use for network requests.

`pocketApiKey` - the API key for Pocket network requests.

`userAgent` - the user agent for network requests.

### Exceptions

`IllegalArgumentException` - if the provided API key or user agent is deemed invalid.