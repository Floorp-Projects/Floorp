[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [RemoteTabsStorage](index.md) / [sync](./sync.md)

# sync

`suspend fun sync(authInfo: `[`SyncAuthInfo`](../../mozilla.components.concept.sync/-sync-auth-info/index.md)`, localId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`SyncStatus`](../../mozilla.components.concept.sync/-sync-status/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/RemoteTabsStorage.kt#L78)

Syncs the remote tabs.

### Parameters

`authInfo` - The authentication information to sync with.

`localId` - Local device ID from FxA.

**Return**
Sync status of OK or Error

