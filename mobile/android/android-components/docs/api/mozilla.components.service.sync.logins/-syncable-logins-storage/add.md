[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStorage](index.md) / [add](./add.md)

# add

`suspend fun add(login: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/SyncableLoginsStorage.kt#L197)

Overrides [LoginsStorage.add](../../mozilla.components.concept.storage/-logins-storage/add.md)

### Exceptions

`IdCollisionException` - if a nonempty id is provided, and

`InvalidRecordException` - if the record is invalid.

`LoginsStorageException` - if the storage is locked, and on unexpected
    errors (IO failure, rust panics, etc)