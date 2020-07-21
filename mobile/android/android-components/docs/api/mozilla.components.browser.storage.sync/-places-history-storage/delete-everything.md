[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesHistoryStorage](index.md) / [deleteEverything](./delete-everything.md)

# deleteEverything

`open suspend fun deleteEverything(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesHistoryStorage.kt#L133)

Overrides [HistoryStorage.deleteEverything](../../mozilla.components.concept.storage/-history-storage/delete-everything.md)

Sync behaviour: will not remove any history from remote devices, but it will prevent deleted
history from returning.

