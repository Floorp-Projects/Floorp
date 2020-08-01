[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [HistoryStorage](index.md) / [recordObservation](./record-observation.md)

# recordObservation

`abstract suspend fun recordObservation(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, observation: `[`PageObservation`](../-page-observation/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/HistoryStorage.kt#L24)

Records an observation about a page.

### Parameters

`uri` - of the page for which to record an observation.

`observation` - a [PageObservation](../-page-observation/index.md) which encapsulates meta data observed about the page.