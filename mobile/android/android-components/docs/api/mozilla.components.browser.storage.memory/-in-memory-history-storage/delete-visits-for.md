[android-components](../../index.md) / [mozilla.components.browser.storage.memory](../index.md) / [InMemoryHistoryStorage](index.md) / [deleteVisitsFor](./delete-visits-for.md)

# deleteVisitsFor

`suspend fun deleteVisitsFor(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-memory/src/main/java/mozilla/components/browser/storage/memory/InMemoryHistoryStorage.kt#L156)

Overrides [HistoryStorage.deleteVisitsFor](../../mozilla.components.concept.storage/-history-storage/delete-visits-for.md)

Remove all history visits for a given [url](../../mozilla.components.concept.storage/-history-storage/delete-visits-for.md#mozilla.components.concept.storage.HistoryStorage$deleteVisitsFor(kotlin.String)/url).

### Parameters

`url` - A page URL for which to remove visits.