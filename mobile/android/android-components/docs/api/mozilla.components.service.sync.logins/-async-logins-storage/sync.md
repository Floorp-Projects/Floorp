[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [AsyncLoginsStorage](index.md) / [sync](./sync.md)

# sync

`abstract fun sync(syncInfo: `[`SyncUnlockInfo`](../-sync-unlock-info.md)`): Deferred<SyncTelemetryPing>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L147)

Synchronizes the logins storage layer with a remote layer.

**RejectsWith**
[SyncAuthInvalidException](../-sync-auth-invalid-exception.md) if authentication needs to be refreshed

**RejectsWith**
[RequestFailedException](../-request-failed-exception.md) if there was a network error during connection.

**RejectsWith**
[LoginsStorageException](../-logins-storage-exception.md) if the storage is locked, and on unexpected
    errors (IO failure, rust panics, etc)

