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

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
