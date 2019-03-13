[android-components](../../index.md) / [mozilla.components.feature.sync](../index.md) / [StorageSync](index.md) / [sync](./sync.md)

# sync

`suspend fun sync(account: `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md)`): `[`SyncResult`](../../mozilla.components.concept.sync/-sync-result.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sync/src/main/java/mozilla/components/feature/sync/StorageSync.kt#L45)

Performs a sync of configured [SyncableStore](../../mozilla.components.concept.sync/-syncable-store/index.md) history instance. This method guarantees that
only one sync may be running at any given time.

### Parameters

`account` - [OAuthAccount](../../mozilla.components.concept.sync/-o-auth-account/index.md) for which to perform a sync.

**Return**
a [SyncResult](../../mozilla.components.concept.sync/-sync-result.md) indicating result of synchronization of configured stores.

