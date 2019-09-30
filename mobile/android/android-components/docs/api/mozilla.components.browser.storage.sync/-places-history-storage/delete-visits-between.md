[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesHistoryStorage](index.md) / [deleteVisitsBetween](./delete-visits-between.md)

# deleteVisitsBetween

`open suspend fun deleteVisitsBetween(startTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, endTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesHistoryStorage.kt#L132)

Overrides [HistoryStorage.deleteVisitsBetween](../../mozilla.components.concept.storage/-history-storage/delete-visits-between.md)

Sync behaviour: may remove history from remote devices, if the removed visits were the only
ones for a URL.

