[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesHistoryStorage](index.md) / [deleteVisit](./delete-visit.md)

# deleteVisit

`open suspend fun deleteVisit(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, timestamp: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesHistoryStorage.kt#L151)

Overrides [HistoryStorage.deleteVisit](../../mozilla.components.concept.storage/-history-storage/delete-visit.md)

Sync behaviour: will remove history from remote devices if this was the only visit for [url](delete-visit.md#mozilla.components.browser.storage.sync.PlacesHistoryStorage$deleteVisit(kotlin.String, kotlin.Long)/url).
Otherwise, remote devices are not affected.

