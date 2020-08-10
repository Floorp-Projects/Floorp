[android-components](../../index.md) / [mozilla.components.service.pocket](../index.md) / [PocketEndpoint](./index.md)

# PocketEndpoint

`class PocketEndpoint` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/pocket/src/main/java/mozilla/components/service/pocket/PocketEndpoint.kt#L20)

Makes requests to the Pocket API and returns the requested data.

**See Also**

[newInstance](new-instance.md)

### Functions

| Name | Summary |
|---|---|
| [getGlobalVideoRecommendations](get-global-video-recommendations.md) | `fun getGlobalVideoRecommendations(): `[`PocketResponse`](../../mozilla.components.service.pocket.net/-pocket-response/index.md)`<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`PocketGlobalVideoRecommendation`](../../mozilla.components.service.pocket.data/-pocket-global-video-recommendation/index.md)`>>`<br>Gets a response, filled with the Pocket global video recommendations from the Pocket API server on success. This network call is synchronous and the results are not cached. The API version of this call is available in the source code at [PocketURLs.VALUE_VIDEO_RECS_VERSION](#). |

### Companion Object Functions

| Name | Summary |
|---|---|
| [newInstance](new-instance.md) | `fun newInstance(client: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, pocketApiKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, userAgent: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`PocketEndpoint`](./index.md)<br>Returns a new instance of [PocketEndpoint](./index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
