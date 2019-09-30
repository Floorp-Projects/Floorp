[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [SyncableStore](./index.md)

# SyncableStore

`interface SyncableStore` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Sync.kt#L48)

Describes a "sync" entry point for a storage layer.

### Functions

| Name | Summary |
|---|---|
| [getHandle](get-handle.md) | `abstract fun getHandle(): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>This should be removed. See: https://github.com/mozilla/application-services/issues/1877 |
| [sync](sync.md) | `abstract suspend fun sync(authInfo: `[`SyncAuthInfo`](../-sync-auth-info/index.md)`): `[`SyncStatus`](../-sync-status/index.md)<br>Performs a sync. |

### Inheritors

| Name | Summary |
|---|---|
| [PlacesBookmarksStorage](../../mozilla.components.browser.storage.sync/-places-bookmarks-storage/index.md) | `open class PlacesBookmarksStorage : `[`PlacesStorage`](../../mozilla.components.browser.storage.sync/-places-storage/index.md)`, `[`BookmarksStorage`](../../mozilla.components.concept.storage/-bookmarks-storage/index.md)`, `[`SyncableStore`](./index.md)<br>Implementation of the [BookmarksStorage](../../mozilla.components.concept.storage/-bookmarks-storage/index.md) which is backed by a Rust Places lib via [PlacesApi](#). |
| [PlacesHistoryStorage](../../mozilla.components.browser.storage.sync/-places-history-storage/index.md) | `open class PlacesHistoryStorage : `[`PlacesStorage`](../../mozilla.components.browser.storage.sync/-places-storage/index.md)`, `[`HistoryStorage`](../../mozilla.components.concept.storage/-history-storage/index.md)`, `[`SyncableStore`](./index.md)<br>Implementation of the [HistoryStorage](../../mozilla.components.concept.storage/-history-storage/index.md) which is backed by a Rust Places lib via [PlacesApi](#). |
| [PlacesStorage](../../mozilla.components.browser.storage.sync/-places-storage/index.md) | `abstract class PlacesStorage : `[`Storage`](../../mozilla.components.concept.storage/-storage/index.md)`, `[`SyncableStore`](./index.md)<br>A base class for concrete implementations of PlacesStorages |
| [SyncableLoginsStore](../../mozilla.components.service.sync.logins/-syncable-logins-store/index.md) | `data class SyncableLoginsStore : `[`SyncableStore`](./index.md)<br>Wraps [AsyncLoginsStorage](../../mozilla.components.service.sync.logins/-async-logins-storage/index.md) instance along with a lazy encryption key. |
