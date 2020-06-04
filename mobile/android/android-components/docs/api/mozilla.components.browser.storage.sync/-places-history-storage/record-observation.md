[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesHistoryStorage](index.md) / [recordObservation](./record-observation.md)

# recordObservation

`open suspend fun recordObservation(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, observation: `[`PageObservation`](../../mozilla.components.concept.storage/-page-observation/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesHistoryStorage.kt#L58)

Overrides [HistoryStorage.recordObservation](../../mozilla.components.concept.storage/-history-storage/record-observation.md)

Records an observation about a page.

### Parameters

`uri` - of the page for which to record an observation.

`observation` - a [PageObservation](../../mozilla.components.concept.storage/-page-observation/index.md) which encapsulates meta data observed about the page.