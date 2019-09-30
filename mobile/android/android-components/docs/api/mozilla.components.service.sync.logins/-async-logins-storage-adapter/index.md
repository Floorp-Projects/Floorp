[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [AsyncLoginsStorageAdapter](./index.md)

# AsyncLoginsStorageAdapter

`open class AsyncLoginsStorageAdapter<T : LoginsStorage> : `[`AsyncLoginsStorage`](../-async-logins-storage/index.md)`, `[`AutoCloseable`](https://developer.android.com/reference/java/lang/AutoCloseable.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L285)

A helper class to wrap a synchronous [LoginsStorage](#) implementation and make it asynchronous.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AsyncLoginsStorageAdapter(wrapped: `[`T`](index.md#T)`)`<br>A helper class to wrap a synchronous [LoginsStorage](#) implementation and make it asynchronous. |

### Functions

| Name | Summary |
|---|---|
| [add](add.md) | `open fun add(login: `[`ServerPassword`](../-server-password.md)`): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>Inserts the provided login into the database, returning it's id. |
| [close](close.md) | `open fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [delete](delete.md) | `open fun delete(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Deletes the password with the given ID. |
| [ensureLocked](ensure-locked.md) | `open fun ensureLocked(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Equivalent to `lock()`, but does not throw in the case that the database is already unlocked. Never rejects. |
| [ensureUnlocked](ensure-unlocked.md) | `open fun ensureUnlocked(encryptionKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>`open fun ensureUnlocked(encryptionKey: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Equivalent to `unlock(encryptionKey)`, but does not throw in the case that the database is already unlocked. |
| [get](get.md) | `open fun get(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`ServerPassword`](../-server-password.md)`?>`<br>Fetches a password from the underlying storage layer by ID. |
| [getHandle](get-handle.md) | `open fun getHandle(): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>This should be removed. See: https://github.com/mozilla/application-services/issues/1877 |
| [isLocked](is-locked.md) | `open fun isLocked(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns `true` if the storage is locked, false otherwise. |
| [list](list.md) | `open fun list(): Deferred<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ServerPassword`](../-server-password.md)`>>`<br>Fetches the full list of passwords from the underlying storage layer. |
| [lock](lock.md) | `open fun lock(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Locks the logins storage. |
| [sync](sync.md) | `open fun sync(syncInfo: `[`SyncUnlockInfo`](../-sync-unlock-info.md)`): Deferred<SyncTelemetryPing>`<br>Synchronizes the logins storage layer with a remote layer. |
| [touch](touch.md) | `open fun touch(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Marks the login with the given ID as `in-use`. |
| [unlock](unlock.md) | `open fun unlock(encryptionKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Unlocks the logins storage using the provided key.`open fun unlock(encryptionKey: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Unlock (open) the database, using a byte string as the key. This is equivalent to calling unlock() after hex-encoding the bytes (lower case hexadecimal characters are used). |
| [update](update.md) | `open fun update(login: `[`ServerPassword`](../-server-password.md)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Updates the fields in the provided record. |
| [wipe](wipe.md) | `open fun wipe(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Delete all login records. These deletions will be synced to the server on the next call to sync. |
| [wipeLocal](wipe-local.md) | `open fun wipeLocal(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Clear out all local state, bringing us back to the state before the first write (or sync). |

### Companion Object Functions

| Name | Summary |
|---|---|
| [forDatabase](for-database.md) | `fun forDatabase(dbPath: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`AsyncLoginsStorageAdapter`](./index.md)`<DatabaseLoginsStorage>`<br>Creates an [AsyncLoginsStorage](../-async-logins-storage/index.md) that is backed by a [DatabaseLoginsStorage](#). |
| [inMemory](in-memory.md) | `fun inMemory(items: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ServerPassword`](../-server-password.md)`>): `[`AsyncLoginsStorageAdapter`](./index.md)`<MemoryLoginsStorage>`<br>Creates an [AsyncLoginsStorage](../-async-logins-storage/index.md) that is backed by a [MemoryLoginsStorage](#). |
