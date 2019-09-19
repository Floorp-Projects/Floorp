[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [AsyncLoginsStorage](index.md) / [unlock](./unlock.md)

# unlock

`abstract fun unlock(encryptionKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L122)

Unlocks the logins storage using the provided key.

**RejectsWith**
[InvalidKeyException](../-invalid-key-exception.md) if the encryption key is wrong, or the db is corrupt

**RejectsWith**
[MismatchedLockException](../-mismatched-lock-exception.md) if we're already unlocked

**RejectsWith**
[LoginsStorageException](../-logins-storage-exception.md) if the storage is locked, and on unexpected
    errors (IO failure, rust panics, etc)

`abstract fun unlock(encryptionKey: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L134)

Unlock (open) the database, using a byte string as the key.
This is equivalent to calling unlock() after hex-encoding the bytes (lower
case hexadecimal characters are used).

**RejectsWith**
[InvalidKeyException](../-invalid-key-exception.md) if the encryption key is wrong, or the db is corrupt

**RejectsWith**
[MismatchedLockException](../-mismatched-lock-exception.md) if the database is already unlocked

**RejectsWith**
[LoginsStorageException](../-logins-storage-exception.md) if the storage is locked, and on unexpected
    errors (IO failure, rust panics, etc)

