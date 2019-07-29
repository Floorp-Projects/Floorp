/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.logins

import kotlinx.coroutines.async
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.plus
import mozilla.appservices.logins.DatabaseLoginsStorage
import mozilla.appservices.logins.LoginsStorage
import mozilla.appservices.logins.MemoryLoginsStorage
import mozilla.components.concept.sync.SyncAuthInfo
import mozilla.components.concept.sync.SyncStatus
import mozilla.components.concept.sync.SyncableStore
import mozilla.appservices.sync15.SyncTelemetryPing

/**
 * This type contains the set of information required to successfully
 * connect to the server and sync.
 */
typealias SyncUnlockInfo = mozilla.appservices.logins.SyncUnlockInfo

/**
 * Raw password data that is stored by the storage implementation.
 */
typealias ServerPassword = mozilla.appservices.logins.ServerPassword

/**
 * The telemetry ping from a successful sync
 */
typealias SyncTelemetryPing = mozilla.appservices.sync15.SyncTelemetryPing

/**
 * The base class of all errors emitted by logins storage.
 *
 * Concrete instances of this class are thrown for operations which are
 * not expected to be handled in a meaningful way by the application.
 *
 * For example, caught Rust panics, SQL errors, failure to generate secure
 * random numbers, etc. are all examples of things which will result in a
 * concrete `LoginsStorageException`.
 */
typealias LoginsStorageException = mozilla.appservices.logins.LoginsStorageException

/**
 * This indicates that the authentication information (e.g. the [SyncUnlockInfo])
 * provided to [AsyncLoginsStorage.sync] is invalid. This often indicates that it's
 * stale and should be refreshed with FxA (however, care should be taken not to
 * get into a loop refreshing this information).
 */
typealias SyncAuthInvalidException = mozilla.appservices.logins.SyncAuthInvalidException

/**
 * This is thrown if `lock()`/`unlock()` pairs don't match up.
 */
typealias MismatchedLockException = mozilla.appservices.logins.MismatchedLockException

/**
 * This is thrown if `update()` is performed with a record whose ID
 * does not exist.
 */
typealias NoSuchRecordException = mozilla.appservices.logins.NoSuchRecordException

/**
 * This is thrown if `add()` is given a record whose `id` is not blank, and
 * collides with a record already known to the storage instance.
 *
 * You can avoid ever worrying about this error by always providing blank
 * `id` property when inserting new records.
 */
typealias IdCollisionException = mozilla.appservices.logins.IdCollisionException

/**
 * This is thrown on attempts to insert or update a record so that it
 * is no longer valid, where "invalid" is defined as such:
 *
 * - A record with a blank `password` is invalid.
 * - A record with a blank `hostname` is invalid.
 * - A record that doesn't have a `formSubmitURL` nor a `httpRealm` is invalid.
 * - A record that has both a `formSubmitURL` and a `httpRealm` is invalid.
 */
typealias InvalidRecordException = mozilla.appservices.logins.InvalidRecordException

/**
 * This error is emitted in two cases:
 *
 * 1. An incorrect key is used to to open the login database
 * 2. The file at the path specified is not a sqlite database.
 *
 * SQLCipher does not give any way to distinguish between these two cases.
 */
typealias InvalidKeyException = mozilla.appservices.logins.InvalidKeyException

/**
 * This error is emitted if a request to a sync server failed.
 */
typealias RequestFailedException = mozilla.appservices.logins.RequestFailedException

/**
 * An interface equivalent to the LoginsStorage interface, but where operations are
 * asynchronous.
 */
@Suppress("TooManyFunctions")
interface AsyncLoginsStorage : AutoCloseable {
    /** Locks the logins storage.
     *
     * @rejectsWith [MismatchedLockException] if we're already locked
     */
    fun lock(): Deferred<Unit>

    /** Unlocks the logins storage using the provided key.
     *
     * @rejectsWith [InvalidKeyException] if the encryption key is wrong, or the db is corrupt
     * @rejectsWith [MismatchedLockException] if we're already unlocked
     * @rejectsWith [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    fun unlock(encryptionKey: String): Deferred<Unit>

    /**
     * Unlock (open) the database, using a byte string as the key.
     * This is equivalent to calling unlock() after hex-encoding the bytes (lower
     * case hexadecimal characters are used).
     *
     * @rejectsWith [InvalidKeyException] if the encryption key is wrong, or the db is corrupt
     * @rejectsWith [MismatchedLockException] if the database is already unlocked
     * @rejectsWith [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    fun unlock(encryptionKey: ByteArray): Deferred<Unit>

    /** Returns `true` if the storage is locked, false otherwise. */
    fun isLocked(): Boolean

    /**
     * Synchronizes the logins storage layer with a remote layer.
     *
     * @rejectsWith [SyncAuthInvalidException] if authentication needs to be refreshed
     * @rejectsWith [RequestFailedException] if there was a network error during connection.
     * @rejectsWith [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    fun sync(syncInfo: SyncUnlockInfo): Deferred<SyncTelemetryPing>

    /**
     * Delete all login records. These deletions will be synced to the server on the next call to sync.
     *
     * @rejectsWith [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    fun wipe(): Deferred<Unit>

    /**
     * Clear out all local state, bringing us back to the state before the first write (or sync).
     *
     * @rejectsWith [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    fun wipeLocal(): Deferred<Unit>

    /**
     * Deletes the password with the given ID.
     *
     * Resolves to true if the deletion did anything.
     *
     * @rejectsWith [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    fun delete(id: String): Deferred<Boolean>

    /**
     * Fetches a password from the underlying storage layer by ID.
     *
     * Resolves to `null` if the record does not exist.
     *
     * @rejectsWith [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    fun get(id: String): Deferred<ServerPassword?>

    /**
     * Marks the login with the given ID as `in-use`.
     *
     * @rejectsWith [NoSuchRecordException] if the login does not exist.
     * @rejectsWith [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    fun touch(id: String): Deferred<Unit>

    /**
     * Fetches the full list of passwords from the underlying storage layer.
     *
     * @rejectsWith [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    fun list(): Deferred<List<ServerPassword>>

    /**
     * Inserts the provided login into the database, returning it's id.
     *
     * This function ignores values in metadata fields (`timesUsed`,
     * `timeCreated`, `timeLastUsed`, and `timePasswordChanged`).
     *
     * If login has an empty id field, then a GUID will be
     * generated automatically. The format of generated guids
     * are left up to the implementation of LoginsStorage (in
     * practice the [DatabaseLoginsStorage] generates 12-character
     * base64url (RFC 4648) encoded strings, and [MemoryLoginsStorage]
     * generates strings using [java.util.UUID.toString])
     *
     * This will return an error result if a GUID is provided but
     * collides with an existing record, or if the provided record
     * is invalid (missing password, hostname, or doesn't have exactly
     * one of formSubmitURL and httpRealm).
     *
     * @rejectsWith [IdCollisionException] if a nonempty id is provided, and
     * @rejectsWith [InvalidRecordException] if the record is invalid.
     * @rejectsWith [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    fun add(login: ServerPassword): Deferred<String>

    /**
     * Updates the fields in the provided record.
     *
     * This will return an error if `login.id` does not refer to
     * a record that exists in the database, or if the provided record
     * is invalid (missing password, hostname, or doesn't have exactly
     * one of formSubmitURL and httpRealm).
     *
     * Like `add`, this function will ignore values in metadata
     * fields (`timesUsed`, `timeCreated`, `timeLastUsed`, and
     * `timePasswordChanged`).
     *
     * @rejectsWith [NoSuchRecordException] if the login does not exist.
     * @rejectsWith [InvalidRecordException] if the update would create an invalid record.
     * @rejectsWith [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    fun update(login: ServerPassword): Deferred<Unit>

    /**
     * Equivalent to `unlock(encryptionKey)`, but does not throw in the case
     * that the database is already unlocked.
     *
     * @rejectsWith [InvalidKeyException] if the encryption key is wrong, or the db is corrupt
     * @rejectsWith [LoginsStorageException] if there was some other error opening the database
     */
    fun ensureUnlocked(encryptionKey: String): Deferred<Unit>

    /**
     * Equivalent to `unlock(encryptionKey)`, but does not throw in the case
     * that the database is already unlocked.
     *
     * @rejectsWith [InvalidKeyException] if the encryption key is wrong, or the db is corrupt
     * @rejectsWith [LoginsStorageException] if there was some other error opening the database
     */
    fun ensureUnlocked(encryptionKey: ByteArray): Deferred<Unit>

    /**
     * Equivalent to `lock()`, but does not throw in the case that
     * the database is already unlocked. Never rejects.
     */
    fun ensureLocked(): Deferred<Unit>

    /**
     * This should be removed. See: https://github.com/mozilla/application-services/issues/1877
     *
     * Note: handles do not remain valid after locking / unlocking the logins database.
     *
     * @return raw internal handle that could be used for referencing underlying logins database.
     * Use it with SyncManager.
     */
    fun getHandle(): Long
}

/**
 * A helper class to wrap a synchronous [LoginsStorage] implementation and make it asynchronous.
 */
@Suppress("TooManyFunctions")
open class AsyncLoginsStorageAdapter<T : LoginsStorage>(private val wrapped: T) : AsyncLoginsStorage, AutoCloseable {
    private val job = SupervisorJob()
    private val scope = CoroutineScope(Dispatchers.IO) + job

    override fun lock(): Deferred<Unit> {
        return scope.async { wrapped.lock() }
    }

    override fun unlock(encryptionKey: String): Deferred<Unit> {
        return scope.async { wrapped.unlock(encryptionKey) }
    }

    override fun unlock(encryptionKey: ByteArray): Deferred<Unit> {
        return scope.async { wrapped.unlock(encryptionKey) }
    }

    override fun ensureLocked(): Deferred<Unit> {
        return scope.async { wrapped.ensureLocked() }
    }

    override fun ensureUnlocked(encryptionKey: String): Deferred<Unit> {
        return scope.async { wrapped.ensureUnlocked(encryptionKey) }
    }

    override fun ensureUnlocked(encryptionKey: ByteArray): Deferred<Unit> {
        return scope.async { wrapped.ensureUnlocked(encryptionKey) }
    }

    override fun wipeLocal(): Deferred<Unit> {
        return scope.async { wrapped.wipeLocal() }
    }

    override fun isLocked(): Boolean {
        return wrapped.isLocked()
    }

    override fun sync(syncInfo: SyncUnlockInfo): Deferred<SyncTelemetryPing> {
        return scope.async {
            wrapped.sync(syncInfo)
        }
    }

    override fun wipe(): Deferred<Unit> {
        return scope.async { wrapped.wipe() }
    }

    override fun delete(id: String): Deferred<Boolean> {
        return scope.async { wrapped.delete(id) }
    }

    override fun get(id: String): Deferred<ServerPassword?> {
        return scope.async { wrapped.get(id) }
    }

    override fun touch(id: String): Deferred<Unit> {
        return scope.async { wrapped.touch(id) }
    }

    override fun list(): Deferred<List<ServerPassword>> {
        return scope.async { wrapped.list() }
    }

    override fun add(login: ServerPassword): Deferred<String> {
        return scope.async { wrapped.add(login) }
    }

    override fun update(login: ServerPassword): Deferred<Unit> {
        return scope.async { wrapped.update(login) }
    }

    override fun getHandle(): Long {
        return wrapped.getHandle()
    }

    override fun close() {
        job.cancel()
        wrapped.close()
    }

    companion object {
        /**
         * Creates an [AsyncLoginsStorage] that is backed by a [DatabaseLoginsStorage].
         */
        fun forDatabase(dbPath: String): AsyncLoginsStorageAdapter<DatabaseLoginsStorage> {
            return AsyncLoginsStorageAdapter(DatabaseLoginsStorage(dbPath))
        }

        /**
         * Creates an [AsyncLoginsStorage] that is backed by a [MemoryLoginsStorage].
         */
        fun inMemory(items: List<ServerPassword>): AsyncLoginsStorageAdapter<MemoryLoginsStorage> {
            return AsyncLoginsStorageAdapter(MemoryLoginsStorage(items))
        }
    }
}

/**
 * Wraps [AsyncLoginsStorage] instance along with a lazy encryption key.
 *
 * This helper class lives here and not alongside [AsyncLoginsStorage] because we don't want to
 * force a `service-sync-logins` dependency (which has a heavy native library dependency) on
 * consumers of [FirefoxSyncFeature].
 */
data class SyncableLoginsStore(
    val store: AsyncLoginsStorage,
    val key: () -> Deferred<String>
) : SyncableStore {
    override suspend fun sync(authInfo: SyncAuthInfo): SyncStatus {
        return try {
            withUnlocked {
                it.sync(authInfo.into()).await()
                SyncStatus.Ok
            }
        } catch (e: LoginsStorageException) {
            SyncStatus.Error(e)
        }
    }

    override fun getHandle(): Long {
        return store.getHandle()
    }

    /**
     * Run some [block] which operates over an unlocked instance of [AsyncLoginsStorage].
     * Database is locked once [block] is done.
     *
     * @throws [InvalidKeyException] if the provided [key] isn't valid.
     */
    suspend fun <T> withUnlocked(block: suspend (AsyncLoginsStorage) -> T): T {

        store.ensureUnlocked(key().await()).await()

        try {
            return block(store)
        } finally {
            store.ensureLocked().await()
        }
    }
}
