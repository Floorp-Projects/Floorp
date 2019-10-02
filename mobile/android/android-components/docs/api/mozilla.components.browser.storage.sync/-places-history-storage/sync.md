[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesHistoryStorage](index.md) / [sync](./sync.md)

# sync

`open suspend fun sync(authInfo: `[`SyncAuthInfo`](../../mozilla.components.concept.sync/-sync-auth-info/index.md)`): `[`SyncStatus`](../../mozilla.components.concept.sync/-sync-status/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesHistoryStorage.kt#L174)

Overrides [SyncableStore.sync](../../mozilla.components.concept.sync/-syncable-store/sync.md)

Runs syncHistory() method on the places Connection

### Parameters

`authInfo` - The authentication information to sync with.

**Return**
Sync status of OK or Error

