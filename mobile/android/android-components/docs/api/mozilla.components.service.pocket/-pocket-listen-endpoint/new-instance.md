[android-components](../../index.md) / [mozilla.components.service.pocket](../index.md) / [PocketListenEndpoint](index.md) / [newInstance](./new-instance.md)

# newInstance

`fun newInstance(client: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, accessToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, userAgent: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`PocketListenEndpoint`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/pocket/src/main/java/mozilla/components/service/pocket/PocketListenEndpoint.kt#L50)

Returns a new instance of [PocketListenEndpoint](index.md).

### Parameters

`client` - the HTTP client to use for network requests.

`accessToken` - the access token for Pocket Listen network requests.

`userAgent` - the user agent for network requests.

### Exceptions

`IllegalArgumentException` - if the provided access token or user agent is deemed invalid.