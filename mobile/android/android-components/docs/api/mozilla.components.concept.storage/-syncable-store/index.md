[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [SyncableStore](./index.md)

# SyncableStore

`interface SyncableStore<AuthInfo>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/SyncableStore.kt#L12)

Describes a "sync" entry point for store which operates over [AuthInfo](index.md#AuthInfo).

### Functions

| Name | Summary |
|---|---|
| [sync](sync.md) | `abstract suspend fun sync(authInfo: `[`AuthInfo`](index.md#AuthInfo)`): `[`SyncStatus`](../-sync-status.md)<br>Performs a sync. |

### Inheritors

| Name | Summary |
|---|---|
| [PlacesHistoryStorage](../../mozilla.components.browser.storage.sync/-places-history-storage/index.md) | `open class PlacesHistoryStorage : `[`HistoryStorage`](../-history-storage/index.md)`, `[`SyncableStore`](./index.md)`<SyncAuthInfo>`<br>Implementation of the [HistoryStorage](../-history-storage/index.md) which is backed by a Rust Places lib via [PlacesConnection](#). |
| [SyncableLoginsStore](../../mozilla.components.service.sync.logins/-syncable-logins-store/index.md) | `data class SyncableLoginsStore : `[`SyncableStore`](./index.md)`<`[`SyncUnlockInfo`](../../mozilla.components.service.sync.logins/-sync-unlock-info.md)`>`<br>Wraps [AsyncLoginsStorage](../../mozilla.components.service.sync.logins/-async-logins-storage/index.md) instance along with a lazy encryption key. |
