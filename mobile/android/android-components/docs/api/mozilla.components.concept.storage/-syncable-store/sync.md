[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [SyncableStore](index.md) / [sync](./sync.md)

# sync

`abstract suspend fun sync(authInfo: `[`AuthInfo`](index.md#AuthInfo)`): `[`SyncStatus`](../-sync-status.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/SyncableStore.kt#L18)

Performs a sync.

### Parameters

`authInfo` - Auth information of type [AuthInfo](index.md#AuthInfo) necessary for syncing this store.

**Return**
[SyncStatus](../-sync-status.md) A status object describing how sync went.

