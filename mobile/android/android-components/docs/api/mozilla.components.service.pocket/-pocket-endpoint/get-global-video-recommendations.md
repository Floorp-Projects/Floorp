[android-components](../../index.md) / [mozilla.components.service.pocket](../index.md) / [PocketEndpoint](index.md) / [getGlobalVideoRecommendations](./get-global-video-recommendations.md)

# getGlobalVideoRecommendations

`@WorkerThread fun getGlobalVideoRecommendations(): `[`PocketResponse`](../../mozilla.components.service.pocket.net/-pocket-response/index.md)`<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`PocketGlobalVideoRecommendation`](../../mozilla.components.service.pocket.data/-pocket-global-video-recommendation/index.md)`>>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/pocket/src/main/java/mozilla/components/service/pocket/PocketEndpoint.kt#L37)

Gets a response, filled with the Pocket global video recommendations from the Pocket API server on success.
This network call is synchronous and the results are not cached. The API version of this call is available in the
source code at [PocketURLs.VALUE_VIDEO_RECS_VERSION](#).

If the API returns unexpectedly formatted results, these entries will be omitted and the rest of the items are
returned.

**Return**
a [PocketResponse.Success](../../mozilla.components.service.pocket.net/-pocket-response/-success/index.md) with the Pocket global video recommendations (the list will never be empty)
or, on error, a [PocketResponse.Failure](../../mozilla.components.service.pocket.net/-pocket-response/-failure.md).

