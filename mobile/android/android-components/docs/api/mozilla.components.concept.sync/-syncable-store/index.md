[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [SyncableStore](./index.md)

# SyncableStore

`interface SyncableStore` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Sync.kt#L28)

Describes a "sync" entry point for a storage layer.

### Functions

| Name | Summary |
|---|---|
| [sync](sync.md) | `abstract suspend fun sync(authInfo: `[`AuthInfo`](../-auth-info/index.md)`): `[`SyncStatus`](../-sync-status/index.md)<br>Performs a sync. |

### Inheritors

| Name | Summary |
|---|---|
| [PlacesHistoryStorage](../../mozilla.components.browser.storage.sync/-places-history-storage/index.md) | `open class PlacesHistoryStorage : `[`HistoryStorage`](../../mozilla.components.concept.storage/-history-storage/index.md)`, `[`SyncableStore`](./index.md)<br>Implementation of the [HistoryStorage](../../mozilla.components.concept.storage/-history-storage/index.md) which is backed by a Rust Places lib via [PlacesConnection](#). |
| [SyncableLoginsStore](../../mozilla.components.service.sync.logins/-syncable-logins-store/index.md) | `data class SyncableLoginsStore : `[`SyncableStore`](./index.md)<br>Wraps [AsyncLoginsStorage](../../mozilla.components.service.sync.logins/-async-logins-storage/index.md) instance along with a lazy encryption key. |
