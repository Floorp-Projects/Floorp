[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [AsyncLoginsStorageAdapter](index.md) / [get](./get.md)

# get

`open fun get(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`ServerPassword`](../-server-password.md)`?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L335)

Overrides [AsyncLoginsStorage.get](../-async-logins-storage/get.md)

Fetches a password from the underlying storage layer by ID.

Resolves to `null` if the record does not exist.

**RejectsWith**
[LoginsStorageException](../-logins-storage-exception.md) if the storage is locked, and on unexpected
    errors (IO failure, rust panics, etc)

