[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [AsyncLoginsStorageAdapter](index.md) / [lock](./lock.md)

# lock

`open fun lock(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L289)

Overrides [AsyncLoginsStorage.lock](../-async-logins-storage/lock.md)

Locks the logins storage.

**RejectsWith**
[MismatchedLockException](../-mismatched-lock-exception.md) if we're already locked

