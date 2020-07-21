[android-components](../../index.md) / [mozilla.components.browser.storage.memory](../index.md) / [InMemoryHistoryStorage](index.md) / [deleteVisit](./delete-visit.md)

# deleteVisit

`suspend fun deleteVisit(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, timestamp: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-memory/src/main/java/mozilla/components/browser/storage/memory/InMemoryHistoryStorage.kt#L162)

Overrides [HistoryStorage.deleteVisit](../../mozilla.components.concept.storage/-history-storage/delete-visit.md)

Remove a specific visit for a given [url](../../mozilla.components.concept.storage/-history-storage/delete-visit.md#mozilla.components.concept.storage.HistoryStorage$deleteVisit(kotlin.String, kotlin.Long)/url).

### Parameters

`url` - A page URL for which to remove a visit.

`timestamp` - A unix timestamp, milliseconds, of a visit to be removed.