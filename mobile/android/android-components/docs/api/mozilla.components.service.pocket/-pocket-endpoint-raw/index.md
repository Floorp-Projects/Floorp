[android-components](../../index.md) / [mozilla.components.service.pocket](../index.md) / [PocketEndpointRaw](./index.md)

# PocketEndpointRaw

`class PocketEndpointRaw` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/pocket/src/main/java/mozilla/components/service/pocket/PocketEndpointRaw.kt#L22)

Make requests to the Pocket endpoint and returns the raw JSON data: this class is intended to be very dumb.

**See Also**

[PocketEndpoint](../-pocket-endpoint/index.md)

[newInstance](new-instance.md)

### Functions

| Name | Summary |
|---|---|
| [getGlobalVideoRecommendations](get-global-video-recommendations.md) | `fun getGlobalVideoRecommendations(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |

### Companion Object Functions

| Name | Summary |
|---|---|
| [newInstance](new-instance.md) | `fun newInstance(client: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, pocketApiKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, userAgent: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`PocketEndpointRaw`](./index.md)<br>Returns a new instance of [PocketEndpointRaw](./index.md). |
