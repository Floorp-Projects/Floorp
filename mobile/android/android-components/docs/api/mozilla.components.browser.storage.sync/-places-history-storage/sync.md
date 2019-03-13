[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesHistoryStorage](index.md) / [sync](./sync.md)

# sync

`open suspend fun sync(authInfo: `[`AuthInfo`](../../mozilla.components.concept.sync/-auth-info/index.md)`): `[`SyncStatus`](../../mozilla.components.concept.sync/-sync-status/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesHistoryStorage.kt#L104)

Overrides [SyncableStore.sync](../../mozilla.components.concept.sync/-syncable-store/sync.md)

Performs a sync.

### Parameters

`authInfo` - Auth information necessary for syncing this store.

**Return**
[SyncStatus](../../mozilla.components.concept.sync/-sync-status/index.md) A status object describing how sync went.

