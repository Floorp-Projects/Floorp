[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesHistoryStorage](index.md) / [deleteVisitsSince](./delete-visits-since.md)

# deleteVisitsSince

`open suspend fun deleteVisitsSince(since: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesHistoryStorage.kt#L145)

Overrides [HistoryStorage.deleteVisitsSince](../../mozilla.components.concept.storage/-history-storage/delete-visits-since.md)

Sync behaviour: may remove history from remote devices, if the removed visits were the only
ones for a URL.

