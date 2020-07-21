[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesHistoryStorage](index.md) / [prune](./prune.md)

# prune

`open suspend fun prune(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesHistoryStorage.kt#L183)

Overrides [HistoryStorage.prune](../../mozilla.components.concept.storage/-history-storage/prune.md)

Should only be called in response to severe disk storage pressure. May delete all of the data,
or some subset of it.
Sync behaviour: will not remove history from remote clients.

