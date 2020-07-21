[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStorage](index.md) / [wipe](./wipe.md)

# wipe

`suspend fun wipe(): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/SyncableLoginsStorage.kt#L140)

Overrides [LoginsStorage.wipe](../../mozilla.components.concept.storage/-logins-storage/wipe.md)

### Exceptions

`LoginsStorageException` - if the storage is locked, and on unexpected
    errors (IO failure, rust panics, etc)