[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStorage](index.md) / [sync](./sync.md)

# sync

`suspend fun sync(syncInfo: `[`SyncUnlockInfo`](../-sync-unlock-info.md)`): SyncTelemetryPing` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/SyncableLoginsStorage.kt#L281)

Synchronizes the logins storage layer with a remote layer.
If synchronizing multiple stores, avoid using this - prefer setting up sync via FxaAccountManager instead.

### Exceptions

`SyncAuthInvalidException` - if authentication needs to be refreshed

`RequestFailedException` - if there was a network error during connection.

`LoginsStorageException` - if the storage is locked, and on unexpected
    errors (IO failure, rust panics, etc)