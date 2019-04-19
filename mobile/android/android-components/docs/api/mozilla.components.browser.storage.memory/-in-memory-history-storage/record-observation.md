[android-components](../../index.md) / [mozilla.components.browser.storage.memory](../index.md) / [InMemoryHistoryStorage](index.md) / [recordObservation](./record-observation.md)

# recordObservation

`suspend fun recordObservation(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, observation: `[`PageObservation`](../../mozilla.components.concept.storage/-page-observation/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-memory/src/main/java/mozilla/components/browser/storage/memory/InMemoryHistoryStorage.kt#L43)

Overrides [HistoryStorage.recordObservation](../../mozilla.components.concept.storage/-history-storage/record-observation.md)

Records an observation about a page.

### Parameters

`uri` - of the page for which to record an observation.

`observation` - a [PageObservation](../../mozilla.components.concept.storage/-page-observation/index.md) which encapsulates meta data observed about the page.