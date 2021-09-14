/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.logins

import android.content.Context
import androidx.annotation.GuardedBy
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.withContext
import mozilla.appservices.logins.LoginStore
import mozilla.appservices.sync15.SyncTelemetryPing
import mozilla.components.concept.storage.EncryptedLogin
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginEntry
import mozilla.components.concept.storage.LoginsStorage
import mozilla.components.concept.sync.SyncableStore
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.utils.logElapsedTime
import org.json.JSONObject
import java.io.Closeable

const val DB_NAME = "logins.sqlite"

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
typealias SyncAuthInvalidException = mozilla.appservices.logins.LoginsStorageException.SyncAuthInvalid

/**
 * This is thrown if `lock()`/`unlock()` pairs don't match up.
 */
typealias MismatchedLockException = mozilla.appservices.logins.LoginsStorageException.MismatchedLock

/**
 * This is thrown if `update()` is performed with a record whose GUID
 * does not exist.
 */
typealias NoSuchRecordException = mozilla.appservices.logins.LoginsStorageException.NoSuchRecord

/**
 * This is thrown on attempts to insert or update a record so that it
 * is no longer valid, where "invalid" is defined as such:
 *
 * - A record with a blank `password` is invalid.
 * - A record with a blank `hostname` is invalid.
 * - A record that doesn't have a `formSubmitURL` nor a `httpRealm` is invalid.
 * - A record that has both a `formSubmitURL` and a `httpRealm` is invalid.
 */
typealias InvalidRecordException = mozilla.appservices.logins.LoginsStorageException.InvalidRecord

/**
 * Error encrypting/decrypting logins data
 */
typealias CryptoException = mozilla.appservices.logins.LoginsStorageException.CryptoException

/**
 * This error is emitted when migrating from an sqlcipher DB in two cases:
 *
 * 1. An incorrect key is used to to open the login database
 * 2. The file at the path specified is not a sqlite database.
 *
 * SQLCipher does not give any way to distinguish between these two cases.
 */
typealias InvalidKeyException = mozilla.appservices.logins.LoginsStorageException.InvalidKey

/**
 * This error is emitted if a request to a sync server failed.
 */
typealias RequestFailedException = mozilla.appservices.logins.LoginsStorageException.RequestFailed

/**
 * Implements [LoginsStorage] and [SyncableStore] using the application-services logins library.
 *
 * Synchronization is handled via the SyncManager by calling [registerWithSyncManager]
 */
class SyncableLoginsStorage(
    private val context: Context,
    private val key: String
) : LoginsStorage, SyncableStore, AutoCloseable {
    private val logger = Logger("SyncableLoginsStorage")
    private val coroutineContext by lazy { Dispatchers.IO }
    private val conn by lazy {
        LoginStorageConnection.init(dbPath = context.getDatabasePath(DB_NAME).absolutePath)
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
    override suspend fun delete(guid: String): Boolean = withContext(coroutineContext) {
        conn.getStorage().delete(guid)
    }

    /**
     * @throws [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    @Throws(LoginsStorageException::class)
    override suspend fun get(guid: String): Login? = withContext(coroutineContext) {
        val encryptedLogin = conn.getStorage().get(guid)?.toEncryptedLogin()
        if(encryptedLogin == null) null else decryptLogin(encryptedLogin)
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
        conn.getStorage().list().map { decryptLogin(it.toEncryptedLogin()) }
    }

    /**
     * @throws [InvalidRecordException] if the record is invalid.
     * @throws [CryptoException] invalid encryption key
     * @throws [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    @Throws(CryptoException::class, InvalidRecordException::class, LoginsStorageException::class)
    override suspend fun add(entry: LoginEntry) = withContext(coroutineContext) {
        conn.getStorage().add(entry.toLoginEntry(), key).toEncryptedLogin()
    }

    /**
     * @throws [NoSuchRecordException] if the login does not exist.
     * @throws [CryptoException] invalid encryption key
     * @throws [InvalidRecordException] if the update would create an invalid record.
     * @throws [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    @Throws(CryptoException::class, NoSuchRecordException::class, InvalidRecordException::class, LoginsStorageException::class)
    override suspend fun update(guid: String, entry: LoginEntry) = withContext(coroutineContext) {
        conn.getStorage().update(guid, entry.toLoginEntry(), key).toEncryptedLogin()
    }

    /**
     * @throws [InvalidRecordException] if the update would create an invalid record.
     * @throws [CryptoException] invalid encryption key
     * @throws [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    @Throws(CryptoException::class, InvalidRecordException::class, LoginsStorageException::class)
    override suspend fun addOrUpdate(entry: LoginEntry) = withContext(coroutineContext) {
        conn.getStorage().addOrUpdate(entry.toLoginEntry(), key).toEncryptedLogin()
    }

    override fun registerWithSyncManager() {
        conn.getStorage().registerWithSyncManager()
    }

    override fun getHandle(): Long {
        throw NotImplementedError("Use registerWithSyncManager instead")
    }

    /**
     * @throws [CryptoException] invalid encryption key
     * @throws [LoginsStorageException] If DB isn't empty during an import; also, on unexpected errors
     * (IO failure, rust panics, etc).
     */
    @Throws(CryptoException::class, LoginsStorageException::class)
    override suspend fun importLoginsAsync(logins: List<Login>): JSONObject = withContext(coroutineContext) {
        JSONObject(conn.getStorage().importMultiple(logins.map { it.toLogin() }, key))
    }

    /**
     * @throws [LoginsStorageException] On unexpected errors (IO failure, rust panics, etc)
     */
    @Throws(LoginsStorageException::class)
    override suspend fun getByBaseDomain(origin: String): List<Login> = withContext(coroutineContext) {
        conn.getStorage().getByBaseDomain(origin).map { decryptLogin(it.toEncryptedLogin()) }
    }

    /**
     * @throws [CryptoException] invalid encryption key
     * @throws [LoginsStorageException] On unexpected errors (IO failure, rust panics, etc)
     */
    @Throws(LoginsStorageException::class)
    override fun findLoginToUpdate(entry: LoginEntry): Login? {
        return conn.getStorage().findLoginToUpdate(entry.toLoginEntry(), key)?.toLogin()
    }

    /**
     * @throws [CryptoException] invalid encryption key
     */
    override fun decryptLogin(login: EncryptedLogin): Login {
        val secFields = mozilla.appservices.logins.decryptFields(login.secFields, key)
        return Login (
            guid = login.guid,
            origin = login.origin,
            username = secFields.username,
            password = secFields.password,
            formActionOrigin = login.formActionOrigin,
            httpRealm = login.httpRealm,
            usernameField = login.usernameField,
            passwordField = login.passwordField,
            timesUsed = login.timesUsed,
            timeCreated = login.timeCreated,
            timeLastUsed = login.timeLastUsed,
            timePasswordChanged = login.timePasswordChanged,
        )
    }

    override fun close() {
        coroutineContext.cancel()
        conn.close()
    }

    companion object {
        fun createKey() = mozilla.appservices.logins.createKey()
    }
}

/**
 * A singleton wrapping a [LoginsStorage] connection.
 */
internal object LoginStorageConnection : Closeable {
    @GuardedBy("this")
    private var storage: LoginStore? = null

    internal fun init(dbPath: String = DB_NAME) = synchronized(this) {
        if (storage == null) {
            storage = LoginStore(dbPath)
        }
        storage
    }

    internal fun getStorage(): LoginStore = synchronized(this) {
        check(storage != null) { "must call init first" }
        return storage!!
    }

    override fun close() = synchronized(this) {
        check(storage != null) { "must call init first" }
        storage!!.close()
        storage = null
    }
}
