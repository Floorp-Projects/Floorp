[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStorage](index.md) / [delete](./delete.md)

# delete

`suspend fun delete(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/SyncableLoginsStorage.kt#L158)

Overrides [LoginsStorage.delete](../../mozilla.components.concept.storage/-logins-storage/delete.md)

### Exceptions

`LoginsStorageException` - if the storage is locked, and on unexpected
    errors (IO failure, rust panics, etc)