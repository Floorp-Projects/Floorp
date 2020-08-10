[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStorage](index.md) / [list](./list.md)

# list

`suspend fun list(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../../mozilla.components.concept.storage/-login/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/SyncableLoginsStorage.kt#L186)

Overrides [LoginsStorage.list](../../mozilla.components.concept.storage/-logins-storage/list.md)

### Exceptions

`LoginsStorageException` - if the storage is locked, and on unexpected
    errors (IO failure, rust panics, etc)