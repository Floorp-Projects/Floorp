[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStore](index.md) / [sync](./sync.md)

# sync

`suspend fun sync(authInfo: `[`SyncUnlockInfo`](../-sync-unlock-info.md)`): `[`SyncStatus`](../../mozilla.components.concept.storage/-sync-status.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L373)

Overrides [SyncableStore.sync](../../mozilla.components.concept.storage/-syncable-store/sync.md)

Performs a sync.

### Parameters

`authInfo` - Auth information of type [AuthInfo](#) necessary for syncing this store.

**Return**
[SyncStatus](../../mozilla.components.concept.storage/-sync-status.md) A status object describing how sync went.

