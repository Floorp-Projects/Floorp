/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.logins

import android.content.Context
import androidx.annotation.GuardedBy
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.withContext
import mozilla.appservices.logins.DatabaseLoginsStorage
import mozilla.appservices.logins.LoginsStorage as RustLoginStorage
import mozilla.appservices.sync15.SyncTelemetryPing
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginsStorage
import mozilla.components.concept.sync.SyncableStore
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.sync.telemetry.SyncTelemetry
import mozilla.components.support.utils.logElapsedTime
import org.json.JSONObject
import java.io.Closeable

const val DB_NAME = "logins.sqlite"

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
 * An implementation of [LoginsStorage] backed by application-services' `logins` library.
 * Synchronization support is provided both directly (via [sync]) when only syncing this storage layer,
 * or via [getHandle] when syncing multiple stores. Use the latter in conjunction with [FxaAccountManager].
 */
@SuppressWarnings("TooManyFunctions")
class SyncableLoginsStorage(
    private val context: Context,
    private val key: String
) : LoginsStorage, SyncableStore, AutoCloseable {
    private val logger = Logger("SyncableLoginsStorage")
    private val coroutineContext by lazy { Dispatchers.IO }

    private val conn by lazy {
        LoginStorageConnection.init(key = key, dbPath = context.getDatabasePath(DB_NAME).absolutePath)
        LoginStorageConnection
    }

    /**
     * "Warms up" this storage layer by establishing the database connection.
     */
    suspend fun warmUp() = withContext(coroutineContext) {
        logElapsedTime(logger, "Warming up storage") { conn }
        Unit
    }

    /**
     * @throws [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    @Throws(LoginsStorageException::class)
    override suspend fun wipe() = withContext(coroutineContext) {
        conn.getStorage().wipe()
    }

    /**
     * @throws [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    @Throws(LoginsStorageException::class)
    override suspend fun wipeLocal() = withContext(coroutineContext) {
        conn.getStorage().wipeLocal()
    }

    /**
     * @throws [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    @Throws(LoginsStorageException::class)
    override suspend fun delete(id: String): Boolean = withContext(coroutineContext) {
        conn.getStorage().delete(id)
    }

    /**
     * @throws [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    @Throws(LoginsStorageException::class)
    override suspend fun get(guid: String): Login? = withContext(coroutineContext) {
        conn.getStorage().get(guid)?.toLogin()
    }

    /**
     * @throws [NoSuchRecordException] if the login does not exist.
     * @throws [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    @Throws(NoSuchRecordException::class, LoginsStorageException::class)
    override suspend fun touch(guid: String) = withContext(coroutineContext) {
        conn.getStorage().touch(guid)
    }

    /**
     * @throws [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    @Throws(LoginsStorageException::class)
    override suspend fun list(): List<Login> = withContext(coroutineContext) {
        conn.getStorage().list().map { it.toLogin() }
    }

    /**
     * @throws [IdCollisionException] if a nonempty id is provided, and
     * @throws [InvalidRecordException] if the record is invalid.
     * @throws [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    @Throws(IdCollisionException::class, InvalidRecordException::class, LoginsStorageException::class)
    override suspend fun add(login: Login): String = withContext(coroutineContext) {
        check(login.guid == null) { "'guid' for a new login must be `null`" }
        conn.getStorage().add(login.toServerPassword())
    }

    /**
     * @throws [NoSuchRecordException] if the login does not exist.
     * @throws [InvalidRecordException] if the update would create an invalid record.
     * @throws [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    @Throws(NoSuchRecordException::class, InvalidRecordException::class, LoginsStorageException::class)
    override suspend fun update(login: Login) = withContext(coroutineContext) {
        conn.getStorage().update(login.toServerPassword())
    }

    override fun getHandle() = conn.getHandle()

    /**
     * @throws [LoginsStorageException] If DB isn't empty during an import; also, on unexpected errors
     * (IO failure, rust panics, etc).
     */
    @Throws(LoginsStorageException::class)
    override suspend fun importLoginsAsync(logins: List<Login>): JSONObject = withContext(coroutineContext) {
        conn.getStorage().importLogins(logins.map { it.toServerPassword() }.toTypedArray())
    }

    /**
     * @throws [InvalidRecordException] On both expected errors (malformed [login], [login]
     * already exists in store, etc. See [InvalidRecordException.reason] for details) and
     * unexpected errors (IO failure, rust panics, etc)
     */
    @Throws(InvalidRecordException::class)
    override suspend fun ensureValid(login: Login) = withContext(coroutineContext) {
        conn.getStorage().ensureValid(login.toServerPassword())
    }

    /**
     * @throws [LoginsStorageException] On unexpected errors (IO failure, rust panics, etc)
     */
    @Throws(LoginsStorageException::class)
    override suspend fun getByBaseDomain(origin: String): List<Login> = withContext(coroutineContext) {
        conn.getStorage().getByBaseDomain(origin).map { it.toLogin() }
    }

    override fun close() {
        coroutineContext.cancel()
        conn.close()
    }

    /**
     * Synchronizes the logins storage layer with a remote layer.
     * If synchronizing multiple stores, avoid using this - prefer setting up sync via FxaAccountManager instead.
     *
     * @throws [SyncAuthInvalidException] if authentication needs to be refreshed
     * @throws [RequestFailedException] if there was a network error during connection.
     * @throws [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    @Throws(SyncAuthInvalidException::class, RequestFailedException::class, LoginsStorageException::class)
    suspend fun sync(syncInfo: SyncUnlockInfo): SyncTelemetryPing = withContext(coroutineContext) {
        conn.getStorage().sync(syncInfo).also {
            SyncTelemetry.processLoginsPing(it)
        }
    }
}

/**
 * A singleton wrapping a [DatabaseLoginsStorage] connection.
 */
internal object LoginStorageConnection : Closeable {
    @GuardedBy("this")
    private var storage: RustLoginStorage? = null

    internal fun init(key: String, dbPath: String = DB_NAME) = synchronized(this) {
        if (storage == null) {
            storage = DatabaseLoginsStorage(dbPath).also {
                it.unlock(key)
            }
        }
    }

    internal fun getStorage(): RustLoginStorage = synchronized(this) {
        check(storage != null) { "must call init first" }
        return storage!!
    }

    internal fun getHandle(): Long = synchronized(this) {
        check(storage != null) { "must call init first" }
        return storage!!.getHandle().also {
            check(it != 0L) { "'handle' unexpectedly 0" }
        }
    }

    override fun close() = synchronized(this) {
        check(storage != null) { "must call init first" }
        storage!!.close()
        storage = null
    }
}
