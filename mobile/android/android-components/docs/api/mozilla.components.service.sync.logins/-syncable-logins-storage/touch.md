[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStorage](index.md) / [touch](./touch.md)

# touch

`suspend fun touch(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/SyncableLoginsStorage.kt#L177)

Overrides [LoginsStorage.touch](../../mozilla.components.concept.storage/-logins-storage/touch.md)

### Exceptions

`NoSuchRecordException` - if the login does not exist.

`LoginsStorageException` - if the storage is locked, and on unexpected
    errors (IO failure, rust panics, etc)