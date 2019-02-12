[android-components](../../index.md) / [mozilla.components.feature.sync](../index.md) / [FirefoxSyncFeature](index.md) / [sync](./sync.md)

# sync

`suspend fun sync(account: `[`FirefoxAccountShaped`](../../mozilla.components.service.fxa/-firefox-account-shaped/index.md)`): `[`SyncResult`](../-sync-result.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sync/src/main/java/mozilla/components/feature/sync/FirefoxSyncFeature.kt#L79)

Performs a sync of configured [SyncableStore](../../mozilla.components.concept.storage/-syncable-store/index.md) history instance. This method guarantees that
only one sync may be running at any given time.

### Parameters

`account` - [FirefoxAccountShaped](../../mozilla.components.service.fxa/-firefox-account-shaped/index.md) for which to perform a sync.

**Return**
a [SyncResult](../-sync-result.md) indicating result of synchronization of configured stores.

