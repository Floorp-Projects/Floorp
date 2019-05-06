[android-components](../../index.md) / [mozilla.components.browser.storage.memory](../index.md) / [InMemoryHistoryStorage](index.md) / [deleteVisitsBetween](./delete-visits-between.md)

# deleteVisitsBetween

`suspend fun deleteVisitsBetween(startTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, endTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-memory/src/main/java/mozilla/components/browser/storage/memory/InMemoryHistoryStorage.kt#L133)

Overrides [HistoryStorage.deleteVisitsBetween](../../mozilla.components.concept.storage/-history-storage/delete-visits-between.md)

Remove history visits in an inclusive range from [startTime](../../mozilla.components.concept.storage/-history-storage/delete-visits-between.md#mozilla.components.concept.storage.HistoryStorage$deleteVisitsBetween(kotlin.Long, kotlin.Long)/startTime) to [endTime](../../mozilla.components.concept.storage/-history-storage/delete-visits-between.md#mozilla.components.concept.storage.HistoryStorage$deleteVisitsBetween(kotlin.Long, kotlin.Long)/endTime).

### Parameters

`startTime` - A unix timestamp, milliseconds.

`endTime` - A unix timestamp, milliseconds.