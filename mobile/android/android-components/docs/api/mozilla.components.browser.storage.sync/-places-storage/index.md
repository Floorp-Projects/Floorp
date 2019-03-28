[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesStorage](./index.md)

# PlacesStorage

`open class PlacesStorage : `[`Storage`](../../mozilla.components.concept.storage/-storage/index.md)`, `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesStorage.kt#L24)

A base class for concrete implementations of PlacesStorages

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PlacesStorage(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`)`<br>A base class for concrete implementations of PlacesStorages |

### Functions

| Name | Summary |
|---|---|
| [cleanup](cleanup.md) | `open fun cleanup(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Cleans up background work and database connections |
| [runMaintenance](run-maintenance.md) | `open suspend fun runMaintenance(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Internal database maintenance tasks. Ideally this should be called once a day. |
| [sync](sync.md) | `open suspend fun sync(authInfo: `[`AuthInfo`](../../mozilla.components.concept.sync/-auth-info/index.md)`): `[`SyncStatus`](../../mozilla.components.concept.sync/-sync-status/index.md)<br>Runs sync() method on the places Connection |

### Inheritors

| Name | Summary |
|---|---|
| [PlacesBookmarksStorage](../-places-bookmarks-storage/index.md) | `open class PlacesBookmarksStorage : `[`PlacesStorage`](./index.md)`, `[`BookmarksStorage`](../../mozilla.components.concept.storage/-bookmarks-storage/index.md)`, `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md)<br>Implementation of the [BookmarksStorage](../../mozilla.components.concept.storage/-bookmarks-storage/index.md) which is backed by a Rust Places lib via [PlacesApi](#). |
| [PlacesHistoryStorage](../-places-history-storage/index.md) | `open class PlacesHistoryStorage : `[`PlacesStorage`](./index.md)`, `[`HistoryStorage`](../../mozilla.components.concept.storage/-history-storage/index.md)`, `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md)<br>Implementation of the [HistoryStorage](../../mozilla.components.concept.storage/-history-storage/index.md) which is backed by a Rust Places lib via [PlacesApi](#). |
