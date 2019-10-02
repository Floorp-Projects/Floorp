[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStore](index.md) / [sync](./sync.md)

# sync

`suspend fun sync(authInfo: `[`SyncAuthInfo`](../../mozilla.components.concept.sync/-sync-auth-info/index.md)`): `[`SyncStatus`](../../mozilla.components.concept.sync/-sync-status/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L392)

Overrides [SyncableStore.sync](../../mozilla.components.concept.sync/-syncable-store/sync.md)

Performs a sync.

### Parameters

`authInfo` - Auth information necessary for syncing this store.

**Return**
[SyncStatus](../../mozilla.components.concept.sync/-sync-status/index.md) A status object describing how sync went.

