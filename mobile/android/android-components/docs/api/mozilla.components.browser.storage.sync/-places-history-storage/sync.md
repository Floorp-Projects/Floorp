[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesHistoryStorage](index.md) / [sync](./sync.md)

# sync

`open suspend fun sync(authInfo: SyncAuthInfo): `[`SyncStatus`](../../mozilla.components.concept.storage/-sync-status.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesHistoryStorage.kt#L106)

Overrides [SyncableStore.sync](../../mozilla.components.concept.storage/-syncable-store/sync.md)

Performs a sync.

### Parameters

`authInfo` - Auth information of type [AuthInfo](#) necessary for syncing this store.

**Return**
[SyncStatus](../../mozilla.components.concept.storage/-sync-status.md) A status object describing how sync went.

