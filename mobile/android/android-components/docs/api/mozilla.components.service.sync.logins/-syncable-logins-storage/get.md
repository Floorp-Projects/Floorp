[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStorage](index.md) / [get](./get.md)

# get

`suspend fun get(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/SyncableLoginsStorage.kt#L167)

Overrides [LoginsStorage.get](../../mozilla.components.concept.storage/-logins-storage/get.md)

### Exceptions

`LoginsStorageException` - if the storage is locked, and on unexpected
    errors (IO failure, rust panics, etc)