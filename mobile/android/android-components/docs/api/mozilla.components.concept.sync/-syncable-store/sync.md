[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [SyncableStore](index.md) / [sync](./sync.md)

# sync

`abstract suspend fun sync(authInfo: `[`SyncAuthInfo`](../-sync-auth-info/index.md)`): `[`SyncStatus`](../-sync-status/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Sync.kt#L55)

Performs a sync.

### Parameters

`authInfo` - Auth information necessary for syncing this store.

**Return**
[SyncStatus](../-sync-status/index.md) A status object describing how sync went.

