[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [AsyncLoginsStorage](index.md) / [lock](./lock.md)

# lock

`abstract fun lock(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L113)

Locks the logins storage.

**RejectsWith**
[MismatchedLockException](../-mismatched-lock-exception.md) if we're already locked

