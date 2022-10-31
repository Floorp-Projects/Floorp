/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.storage

import kotlinx.coroutines.Deferred

/**
 * A login stored in the database
 */
data class Login(
    /**
     * The unique identifier for this login entry.
     */
    val guid: String,
    /**
     * The username for this login entry.
     */
    val username: String,
    /**
     * The password for this login entry.
     */
    val password: String,
    /**
     * The origin this login entry applies to.
     */
    val origin: String,
    /**
     * The origin this login entry was submitted to.
     * This only applies to form-based login entries.
     * It's derived from the action attribute set on the form element.
     */
    val formActionOrigin: String? = null,
    /**
     * The HTTP realm this login entry was requested for.
     * This only applies to non-form-based login entries.
     * It's derived from the WWW-Authenticate header set in a HTTP 401
     * response, see RFC2617 for details.
     */
    val httpRealm: String? = null,
    /**
     * HTML field associated with the [username].
     */
    val usernameField: String = "",
    /**
     * HTML field associated with the [password].
     */
    val passwordField: String = "",
    /**
     * Number of times this password has been used.
     */
    val timesUsed: Long = 0L,
    /**
     * Time of creation in milliseconds from the unix epoch.
     */
    val timeCreated: Long = 0L,
    /**
     * Time of last use in milliseconds from the unix epoch.
     */
    val timeLastUsed: Long = 0L,
    /**
     * Time of last password change in milliseconds from the unix epoch.
     */
    val timePasswordChanged: Long = 0L,
) {
    /**
     * Converts [Login] into a [LoginEntry].
     */
    fun toEntry() = LoginEntry(
        origin = origin,
        formActionOrigin = formActionOrigin,
        httpRealm = httpRealm,
        usernameField = usernameField,
        passwordField = passwordField,
        username = username,
        password = password,
    )
}

/**
 * Login autofill entry
 *
 * This contains the data needed to handle autofill but not the data related to
 * the DB record.  [LoginsStorage] methods that save data typically input
 * [LoginEntry] instances.  This allows the storage backend handle
 * dupe-checking issues like determining which login record should be updated
 * for a given [LoginEntry].  [LoginEntry] also represents the login data
 * that's editable in the API.
 *
 * All fields have the same meaning as in [Login].
 */
data class LoginEntry(
    val origin: String,
    val formActionOrigin: String? = null,
    val httpRealm: String? = null,
    val usernameField: String = "",
    val passwordField: String = "",
    val username: String,
    val password: String,
)

/**
 * Login where the sensitive data is the encrypted.
 *
 * This have the same fields as [Login] except username and password is replaced with [secFields]
 */
data class EncryptedLogin(
    val guid: String,
    val origin: String,
    val formActionOrigin: String? = null,
    val httpRealm: String? = null,
    val usernameField: String = "",
    val passwordField: String = "",
    val timesUsed: Long = 0L,
    val timeCreated: Long = 0L,
    val timeLastUsed: Long = 0L,
    val timePasswordChanged: Long = 0L,
    val secFields: String,
)

/**
 * An interface describing a storage layer for logins/passwords.
 */
interface LoginsStorage : AutoCloseable {
    /**
     * Deletes all login records. These deletions will be synced to the server on the next call to sync.
     */
    suspend fun wipe()

    /**
     * Clears out all local state, bringing us back to the state before the first write (or sync).
     */
    suspend fun wipeLocal()

    /**
     * Deletes the login with the given GUID.
     *
     * @return True if the deletion did anything, false otherwise.
     */
    suspend fun delete(guid: String): Boolean

    /**
     * Fetches a password from the underlying storage layer by its GUID
     *
     * @param guid Unique identifier for the desired record.
     * @return [Login] record, or `null` if the record does not exist.
     */
    suspend fun get(guid: String): Login?

    /**
     * Marks that a login has been used
     *
     * @param guid Unique identifier for the desired record.
     */
    suspend fun touch(guid: String)

    /**
     * Fetches the full list of logins from the underlying storage layer.
     *
     * @return A list of stored [Login] records.
     */
    suspend fun list(): List<Login>

    /**
     * Calculate how we should save a login
     *
     * For a [LoginEntry] to save find an existing [Login] to be update (if
     * any).
     *
     * @param entry [LoginEntry] being saved
     * @return [Login] that should be updated, or null if the login should be added
     */
    suspend fun findLoginToUpdate(entry: LoginEntry): Login?

    /**
     * Inserts the provided login into the database

     * This will return an error result if the provided record is invalid
     * (missing password, origin, or doesn't have exactly one of formSubmitURL
     * and httpRealm).
     *
     * @param login [LoginEntry] to add.
     * @return [EncryptedLogin] that was added
     */
    suspend fun add(entry: LoginEntry): EncryptedLogin

    /**
     * Updates an existing login in the database
     *
     * This will throw if `guid` does not refer to a record that exists in the
     * database, or if the provided record is invalid (missing password,
     * origin, or doesn't have exactly one of formSubmitURL and httpRealm).
     *
     * @param guid Unique identifier for the record
     * @param login [LoginEntry] to add.
     * @return [EncryptedLogin] that was added
     */
    suspend fun update(guid: String, entry: LoginEntry): EncryptedLogin

    /**
     * Checks if a record exists for a [LoginEntry] and calls either add() or update()
     *
     * This will throw if the provided record is invalid (missing password,
     * origin, or doesn't have exactly one of formSubmitURL and httpRealm).
     *
     * @param login [LoginEntry] to add or update.
     * @return [EncryptedLogin] that was added
     */
    suspend fun addOrUpdate(entry: LoginEntry): EncryptedLogin

    /**
     * Bulk-import of a list of [Login].
     * Storage must be empty; implementations expected to throw otherwise.
     *
     * This method exists to support the Fennic -> Fenix migration.  It needs
     * to input [Login] instances in order to ensure the imported logins get
     * the same GUID.
     *
     * @param logins A list of [Login] records to be imported.
     */
    suspend fun importLoginsAsync(logins: List<Login>)

    /**
     * Fetch the list of logins for some origin from the underlying storage layer.
     *
     * @param origin A host name used to look up logins
     * @return A list of [Login] objects, representing matching logins.
     */
    suspend fun getByBaseDomain(origin: String): List<Login>

    /**
     * Decrypt an [EncryptedLogin]
     *
     * @param login [EncryptedLogin] to decrypt
     * @return [Login] with decrypted data
     */
    suspend fun decryptLogin(login: EncryptedLogin): Login
}

/**
 * Validates a [LoginEntry] that will be saved and calculates if saving it
 * would update an existing [Login] or create a new one.
 */
interface LoginValidationDelegate {
    /**
     * The result of validating a given [Login] against currently stored [Login]s.  This will
     * include whether it can be created, updated, or neither, along with an explanation of any errors.
     */
    sealed class Result {
        /**
         * Indicates that the [Login] does not currently exist in the storage, and a new entry
         * with its information can be made.
         */
        object CanBeCreated : Result()

        /**
         * Indicates that a matching [Login] was found in storage, and the [Login] can be used
         * to update its information.
         */
        data class CanBeUpdated(val foundLogin: Login) : Result()
    }

    /**
     *
     * Checks whether a [login] should be saved or updated.
     *
     * @returns a [Result], detailing whether a [login] should be saved or updated.
     */
    fun shouldUpdateOrCreateAsync(entry: LoginEntry): Deferred<Result>
}

/**
 * Used to handle [Login] storage so that the underlying engine doesn't have to. An instance of
 * this should be attached to the Gecko runtime in order to be used.
 */
interface LoginStorageDelegate {
    /**
     * Called after a [login] has been autofilled into web content.
     */
    fun onLoginUsed(login: Login)

    /**
     * Given a [domain], returns the matching [Login]s found in [loginStorage].
     *
     * This is called when the engine believes a field should be autofilled.
     */
    fun onLoginFetch(domain: String): Deferred<List<Login>>

    /**
     * Called when a [LogenEntry] should be added or updated.
     */
    fun onLoginSave(login: LoginEntry)
}
