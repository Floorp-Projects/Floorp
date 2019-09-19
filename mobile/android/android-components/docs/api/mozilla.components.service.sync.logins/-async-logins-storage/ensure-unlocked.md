[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [AsyncLoginsStorage](index.md) / [ensureUnlocked](./ensure-unlocked.md)

# ensureUnlocked

`abstract fun ensureUnlocked(encryptionKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L253)
`abstract fun ensureUnlocked(encryptionKey: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L262)

Equivalent to `unlock(encryptionKey)`, but does not throw in the case
that the database is already unlocked.

**RejectsWith**
[InvalidKeyException](../-invalid-key-exception.md) if the encryption key is wrong, or the db is corrupt

**RejectsWith**
[LoginsStorageException](../-logins-storage-exception.md) if there was some other error opening the database

