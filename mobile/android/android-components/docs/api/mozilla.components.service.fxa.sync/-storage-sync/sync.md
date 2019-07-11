[android-components](../../index.md) / [mozilla.components.service.fxa.sync](../index.md) / [StorageSync](index.md) / [sync](./sync.md)

# sync

`suspend fun sync(authInfo: `[`SyncAuthInfo`](../../mozilla.components.concept.sync/-sync-auth-info/index.md)`): `[`SyncResult`](../../mozilla.components.concept.sync/-sync-result.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sync/StorageSync.kt#L44)

Performs a sync of configured [SyncableStore](../../mozilla.components.concept.sync/-syncable-store/index.md) history instance. This method guarantees that
only one sync may be running at any given time.

### Parameters

`authInfo` - [SyncAuthInfo](../../mozilla.components.concept.sync/-sync-auth-info/index.md) that will be used to authenticate synchronization.

**Return**
a [SyncResult](../../mozilla.components.concept.sync/-sync-result.md) indicating result of synchronization of configured stores.

