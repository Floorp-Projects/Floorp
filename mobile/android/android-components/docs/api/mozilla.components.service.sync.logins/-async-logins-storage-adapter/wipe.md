[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [AsyncLoginsStorageAdapter](index.md) / [wipe](./wipe.md)

# wipe

`open fun wipe(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L327)

Overrides [AsyncLoginsStorage.wipe](../-async-logins-storage/wipe.md)

Delete all login records. These deletions will be synced to the server on the next call to sync.

**RejectsWith**
[LoginsStorageException](../-logins-storage-exception.md) if the storage is locked, and on unexpected
    errors (IO failure, rust panics, etc)

