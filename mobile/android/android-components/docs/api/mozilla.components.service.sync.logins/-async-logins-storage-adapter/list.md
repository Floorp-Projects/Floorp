[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [AsyncLoginsStorageAdapter](index.md) / [list](./list.md)

# list

`open fun list(): Deferred<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ServerPassword`](../-server-password.md)`>>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L343)

Overrides [AsyncLoginsStorage.list](../-async-logins-storage/list.md)

Fetches the full list of passwords from the underlying storage layer.

**RejectsWith**
[LoginsStorageException](../-logins-storage-exception.md) if the storage is locked, and on unexpected
    errors (IO failure, rust panics, etc)

