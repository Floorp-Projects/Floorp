[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStorage](index.md) / [update](./update.md)

# update

`suspend fun update(login: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/SyncableLoginsStorage.kt#L223)

Overrides [LoginsStorage.update](../../mozilla.components.concept.storage/-logins-storage/update.md)

### Exceptions

`NoSuchRecordException` - if the login does not exist.

`InvalidRecordException` - if the update would create an invalid record.

`LoginsStorageException` - if the storage is locked, and on unexpected
    errors (IO failure, rust panics, etc)