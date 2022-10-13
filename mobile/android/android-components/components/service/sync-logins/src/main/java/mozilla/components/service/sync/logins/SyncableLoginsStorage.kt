/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.logins

import android.content.Context
import androidx.annotation.GuardedBy
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withContext
import mozilla.appservices.logins.DatabaseLoginsStorage
import mozilla.appservices.logins.migrateLoginsWithMetrics
import mozilla.components.concept.storage.EncryptedLogin
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginEntry
import mozilla.components.concept.storage.LoginsStorage
import mozilla.components.concept.sync.SyncableStore
import mozilla.components.lib.dataprotect.SecureAbove22Preferences
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.utils.logElapsedTime
import java.io.Closeable

// Older database that was encrypted using SQLCipher
const val DB_NAME_SQLCIPHER = "logins.sqlite"

// Current database
const val DB_NAME = "logins2.sqlite"

// Prefs key that we stored the old SQLCipher encryption key
const val ENCRYPTION_KEY_SQLCIPHER = "passwords"

// Name of our preferences file
const val PREFS_NAME = "logins"

// SQLCipher migration status.
//   - 0 / unset: We haven't done the SQLCipher migration
//   - 1: We performed v1 of the SQLCipher migration
//
// If we discover errors in the migration later, then we can bump this number
// and potentially write code to recover data for users who ran the v1
// migration.
const val SQL_CIPHER_MIGRATION = "sql-cipher-migration"

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
typealias IncorrectKey = mozilla.appservices.logins.LoginsStorageException.IncorrectKey

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
    private val securePrefs: Lazy<SecureAbove22Preferences>,
) : LoginsStorage, SyncableStore, AutoCloseable {
    private val plaintextPrefs by lazy { context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE) }
    private val logger = Logger("SyncableLoginsStorage")
    private val coroutineContext by lazy { Dispatchers.IO }
    val crypto by lazy { LoginsCrypto(context, securePrefs.value, this) }

    internal val conn by lazy {
        // Migration must run before we initialize the database.
        runBlocking(coroutineContext) {
            migrateSQLCipherDBIfNeeded()
        }
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
        conn.getStorage().get(guid)?.toEncryptedLogin()?.let { crypto.decryptLogin(it) }
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
        val key = crypto.getOrGenerateKey()
        conn.getStorage().list().map { crypto.decryptLogin(it.toEncryptedLogin(), key) }
    }

    /**
     * @throws [InvalidRecordException] if the record is invalid.
     * @throws [IncorrectKey] if the encryption key can't decrypt the login
     * @throws [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    @Throws(IncorrectKey::class, InvalidRecordException::class, LoginsStorageException::class)
    override suspend fun add(entry: LoginEntry) = withContext(coroutineContext) {
        conn.getStorage().add(entry.toLoginEntry(), crypto.getOrGenerateKey().key).toEncryptedLogin()
    }

    /**
     * @throws [NoSuchRecordException] if the login does not exist.
     * @throws [IncorrectKey] if the encryption key can't decrypt the login
     * @throws [InvalidRecordException] if the update would create an invalid record.
     * @throws [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    @Throws(
        IncorrectKey::class,
        NoSuchRecordException::class,
        InvalidRecordException::class,
        LoginsStorageException::class,
    )
    override suspend fun update(guid: String, entry: LoginEntry) = withContext(coroutineContext) {
        conn.getStorage().update(guid, entry.toLoginEntry(), crypto.getOrGenerateKey().key).toEncryptedLogin()
    }

    /**
     * @throws [InvalidRecordException] if the update would create an invalid record.
     * @throws [IncorrectKey] if the encryption key can't decrypt the login
     * @throws [LoginsStorageException] if the storage is locked, and on unexpected
     *              errors (IO failure, rust panics, etc)
     */
    @Throws(IncorrectKey::class, InvalidRecordException::class, LoginsStorageException::class)
    override suspend fun addOrUpdate(entry: LoginEntry) = withContext(coroutineContext) {
        conn.getStorage().addOrUpdate(entry.toLoginEntry(), crypto.getOrGenerateKey().key).toEncryptedLogin()
    }

    override fun registerWithSyncManager() {
        conn.getStorage().registerWithSyncManager()
    }

    /**
     * @throws [IncorrectKey] if the encryption key can't decrypt the login
     * @throws [LoginsStorageException] If DB isn't empty during an import; also, on unexpected errors
     * (IO failure, rust panics, etc).
     */
    @Throws(IncorrectKey::class, LoginsStorageException::class)
    override suspend fun importLoginsAsync(logins: List<Login>): Unit = withContext(coroutineContext) {
        conn.getStorage().importMultiple(logins.map { it.toLogin() }, crypto.getOrGenerateKey().key)
    }

    /**
     * @throws [LoginsStorageException] On unexpected errors (IO failure, rust panics, etc)
     */
    @Throws(LoginsStorageException::class)
    override suspend fun getByBaseDomain(origin: String): List<Login> = withContext(coroutineContext) {
        val key = crypto.getOrGenerateKey()
        conn.getStorage().getByBaseDomain(origin).map { crypto.decryptLogin(it.toEncryptedLogin(), key) }
    }

    /**
     * @throws [IncorrectKey] if the encryption key can't decrypt the login
     * @throws [LoginsStorageException] On unexpected errors (IO failure, rust panics, etc)
     */
    @Throws(LoginsStorageException::class)
    override suspend fun findLoginToUpdate(entry: LoginEntry): Login? = withContext(coroutineContext) {
        conn.getStorage().findLoginToUpdate(entry.toLoginEntry(), crypto.getOrGenerateKey().key)?.toLogin()
    }

    /**
     * @throws [IncorrectKey] if the encryption key can't decrypt the login
     */
    override suspend fun decryptLogin(login: EncryptedLogin) = crypto.decryptLogin(login)

    override fun close() {
        coroutineContext.cancel()
        conn.close()
    }

    private suspend fun migrateSQLCipherDBIfNeeded() {
        // Migrate the SQLCipher DB if needed
        val sqlcipherKey = securePrefs.value.getString(ENCRYPTION_KEY_SQLCIPHER) ?: return

        val version = plaintextPrefs.getInt(SQL_CIPHER_MIGRATION, 0)

        if (version == 0) {
            try {
                migrateLoginsWithMetrics(
                    context.getDatabasePath(DB_NAME).absolutePath,
                    crypto.getOrGenerateKey().key,
                    context.getDatabasePath(DB_NAME_SQLCIPHER).absolutePath,
                    sqlcipherKey,
                )
                // Note: DatabaseLoginsStorage.migrateLogins, defined in
                // application-services, is responsible for reporting the migration
                // metrics
            } finally {
                // Set the new version regardless of if the migration
                // succeeded.  If it failed, it's just going to fail until we
                // update the code and bump the version number.
                plaintextPrefs.edit().putInt(SQL_CIPHER_MIGRATION, 1).apply()
            }
        }
    }
}

/**
 * A singleton wrapping a [LoginsStorage] connection.
 */
internal object LoginStorageConnection : Closeable {
    @GuardedBy("this")
    private var storage: DatabaseLoginsStorage? = null

    internal fun init(dbPath: String = DB_NAME) = synchronized(this) {
        if (storage == null) {
            storage = DatabaseLoginsStorage(dbPath)
        }
        storage
    }

    internal fun getStorage(): DatabaseLoginsStorage = synchronized(this) {
        check(storage != null) { "must call init first" }
        return storage!!
    }

    override fun close() = synchronized(this) {
        check(storage != null) { "must call init first" }
        storage!!.close()
        storage = null
    }
}
