[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStorage](index.md) / [wipeLocal](./wipe-local.md)

# wipeLocal

`suspend fun wipeLocal(): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/SyncableLoginsStorage.kt#L149)

Overrides [LoginsStorage.wipeLocal](../../mozilla.components.concept.storage/-logins-storage/wipe-local.md)

### Exceptions

`LoginsStorageException` - if the storage is locked, and on unexpected
    errors (IO failure, rust panics, etc)