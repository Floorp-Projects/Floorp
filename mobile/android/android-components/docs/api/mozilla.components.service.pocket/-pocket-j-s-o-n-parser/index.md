[android-components](../../index.md) / [mozilla.components.service.pocket](../index.md) / [PocketJSONParser](./index.md)

# PocketJSONParser

`class PocketJSONParser` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/pocket/src/main/java/mozilla/components/service/pocket/PocketJSONParser.kt#L18)

Holds functions that parse the JSON returned by the Pocket API and converts them to more usable Kotlin types.

### Functions

| Name | Summary |
|---|---|
| [jsonToGlobalVideoRecommendations](json-to-global-video-recommendations.md) | `fun jsonToGlobalVideoRecommendations(jsonStr: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`PocketGlobalVideoRecommendation`](../../mozilla.components.service.pocket.data/-pocket-global-video-recommendation/index.md)`>?` |

### Companion Object Properties

| Name | Summary |
|---|---|
| [KEY_VIDEO_RECOMMENDATIONS_INNER](-k-e-y_-v-i-d-e-o_-r-e-c-o-m-m-e-n-d-a-t-i-o-n-s_-i-n-n-e-r.md) | `const val KEY_VIDEO_RECOMMENDATIONS_INNER: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [newInstance](new-instance.md) | `fun newInstance(): `[`PocketJSONParser`](./index.md)<br>Returns a new instance of [PocketJSONParser](./index.md). |
