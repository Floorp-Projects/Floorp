[android-components](../../index.md) / [mozilla.components.service.pocket](../index.md) / [PocketJSONParser](index.md) / [jsonToGlobalVideoRecommendations](./json-to-global-video-recommendations.md)

# jsonToGlobalVideoRecommendations

`fun jsonToGlobalVideoRecommendations(jsonStr: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`PocketGlobalVideoRecommendation`](../../mozilla.components.service.pocket.data/-pocket-global-video-recommendation/index.md)`>?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/pocket/src/main/java/mozilla/components/service/pocket/PocketJSONParser.kt#L23)

**Return**
The videos, removing entries that are invalid, or null on error; the list will never be empty.

