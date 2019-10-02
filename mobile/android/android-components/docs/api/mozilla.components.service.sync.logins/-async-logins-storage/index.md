[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [AsyncLoginsStorage](./index.md)

# AsyncLoginsStorage

`interface AsyncLoginsStorage : `[`AutoCloseable`](https://developer.android.com/reference/java/lang/AutoCloseable.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L108)

An interface equivalent to the LoginsStorage interface, but where operations are
asynchronous.

### Functions

| Name | Summary |
|---|---|
| [add](add.md) | `abstract fun add(login: `[`ServerPassword`](../-server-password.md)`): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>Inserts the provided login into the database, returning it's id. |
| [delete](delete.md) | `abstract fun delete(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Deletes the password with the given ID. |
| [ensureLocked](ensure-locked.md) | `abstract fun ensureLocked(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Equivalent to `lock()`, but does not throw in the case that the database is already unlocked. Never rejects. |
| [ensureUnlocked](ensure-unlocked.md) | `abstract fun ensureUnlocked(encryptionKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>`abstract fun ensureUnlocked(encryptionKey: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Equivalent to `unlock(encryptionKey)`, but does not throw in the case that the database is already unlocked. |
| [get](get.md) | `abstract fun get(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`ServerPassword`](../-server-password.md)`?>`<br>Fetches a password from the underlying storage layer by ID. |
| [getHandle](get-handle.md) | `abstract fun getHandle(): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>This should be removed. See: https://github.com/mozilla/application-services/issues/1877 |
| [isLocked](is-locked.md) | `abstract fun isLocked(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns `true` if the storage is locked, false otherwise. |
| [list](list.md) | `abstract fun list(): Deferred<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ServerPassword`](../-server-password.md)`>>`<br>Fetches the full list of passwords from the underlying storage layer. |
| [lock](lock.md) | `abstract fun lock(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Locks the logins storage. |
| [sync](sync.md) | `abstract fun sync(syncInfo: `[`SyncUnlockInfo`](../-sync-unlock-info.md)`): Deferred<SyncTelemetryPing>`<br>Synchronizes the logins storage layer with a remote layer. |
| [touch](touch.md) | `abstract fun touch(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Marks the login with the given ID as `in-use`. |
| [unlock](unlock.md) | `abstract fun unlock(encryptionKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Unlocks the logins storage using the provided key.`abstract fun unlock(encryptionKey: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Unlock (open) the database, using a byte string as the key. This is equivalent to calling unlock() after hex-encoding the bytes (lower case hexadecimal characters are used). |
| [update](update.md) | `abstract fun update(login: `[`ServerPassword`](../-server-password.md)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Updates the fields in the provided record. |
| [wipe](wipe.md) | `abstract fun wipe(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Delete all login records. These deletions will be synced to the server on the next call to sync. |
| [wipeLocal](wipe-local.md) | `abstract fun wipeLocal(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Clear out all local state, bringing us back to the state before the first write (or sync). |

### Inheritors

| Name | Summary |
|---|---|
| [AsyncLoginsStorageAdapter](../-async-logins-storage-adapter/index.md) | `open class AsyncLoginsStorageAdapter<T : LoginsStorage> : `[`AsyncLoginsStorage`](./index.md)`, `[`AutoCloseable`](https://developer.android.com/reference/java/lang/AutoCloseable.html)<br>A helper class to wrap a synchronous [LoginsStorage](#) implementation and make it asynchronous. |
