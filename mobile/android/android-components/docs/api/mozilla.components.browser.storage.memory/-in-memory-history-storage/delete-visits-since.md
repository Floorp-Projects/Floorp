[android-components](../../index.md) / [mozilla.components.browser.storage.memory](../index.md) / [InMemoryHistoryStorage](index.md) / [deleteVisitsSince](./delete-visits-since.md)

# deleteVisitsSince

`suspend fun deleteVisitsSince(since: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-memory/src/main/java/mozilla/components/browser/storage/memory/InMemoryHistoryStorage.kt#L140)

Overrides [HistoryStorage.deleteVisitsSince](../../mozilla.components.concept.storage/-history-storage/delete-visits-since.md)

Remove history visits in an inclusive range from [since](../../mozilla.components.concept.storage/-history-storage/delete-visits-since.md#mozilla.components.concept.storage.HistoryStorage$deleteVisitsSince(kotlin.Long)/since) to now.

### Parameters

`since` - A unix timestamp, milliseconds.