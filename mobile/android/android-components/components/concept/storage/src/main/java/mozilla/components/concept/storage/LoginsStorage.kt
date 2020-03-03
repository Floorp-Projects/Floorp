/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.storage

import kotlinx.coroutines.Deferred
import org.json.JSONObject

/**
 * An interface describing a storage layer for logins/passwords.
 */
@SuppressWarnings("TooManyFunctions")
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
     * Deletes the password with the given ID.
     *
     * @return True if the deletion did anything, false otherwise.
     */
    suspend fun delete(id: String): Boolean

    /**
     * Fetches a password from the underlying storage layer by its unique identifier.
     *
     * @param guid Unique identifier for the desired record.
     * @return [Login] record, or `null` if the record does not exist.
     */
    suspend fun get(guid: String): Login?

    /**
     * Marks the login with the given [guid] as `in-use`.
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
     * Inserts the provided login into the database, returning it's id.
     *
     * This function ignores values in metadata fields (`timesUsed`,
     * `timeCreated`, `timeLastUsed`, and `timePasswordChanged`).
     *
     * If login has an empty id field, then a GUID will be
     * generated automatically. The format of generated guids
     * are left up to the implementation of LoginsStorage (in
     * practice the [DatabaseLoginsStorage] generates 12-character
     * base64url (RFC 4648) encoded strings.
     *
     * This will return an error result if a GUID is provided but
     * collides with an existing record, or if the provided record
     * is invalid (missing password, origin, or doesn't have exactly
     * one of formSubmitURL and httpRealm).
     *
     * @param login A [Login] record to add.
     * @return A `guid` for the created record.
     */
    suspend fun add(login: Login): String

    /**
     * Updates the fields in the provided record.
     *
     * This will throw if `login.id` does not refer to
     * a record that exists in the database, or if the provided record
     * is invalid (missing password, origin, or doesn't have exactly
     * one of formSubmitURL and httpRealm).
     *
     * Like `add`, this function will ignore values in metadata
     * fields (`timesUsed`, `timeCreated`, `timeLastUsed`, and
     * `timePasswordChanged`).
     *
     * @param login A [Login] record instance to update.
     */
    suspend fun update(login: Login)

    /**
     * Bulk-import of a list of [Login].
     * Storage must be empty; implementations expected to throw otherwise.
     *
     * @param logins A list of [Login] records to be imported.
     * @return JSON object with detailed information about imported logins.
     */
    suspend fun importLoginsAsync(logins: List<Login>): JSONObject

    /**
     * Checks if login already exists and is valid. Implementations expected to throw for invalid [login].
     *
     * @param login A [Login] record to validate.
     */
    suspend fun ensureValid(login: Login)

    /**
     * Fetch the list of logins for some origin from the underlying storage layer.
     *
     * @param origin A host name used to look up [Login] records.
     * @return A list of [Login] objects, representing matching logins.
     */
    suspend fun getByBaseDomain(origin: String): List<Login>
}

/**
 * Represents a login that can be used by autofill APIs.
 *
 * Note that much of this information can be partial (e.g., a user might save a password with a
 * blank username).
 */
data class Login(
    /**
     * The unique identifier for this login entry.
     */
    val guid: String? = null,
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
     * The username for this login entry.
     */
    val username: String,
    /**
     * The password for this login entry.
     */
    val password: String,
    /**
     * Number of times this password has been used.
     */
    val timesUsed: Int = 0,
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
    /**
     * HTML field associated with the [username].
     */
    val usernameField: String? = null,
    /**
     * HTML field associated with the [password].
     */
    val passwordField: String? = null
)

/**
 * Provides a method for checking whether or not a given login can be stored.
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
        object CanBeUpdated : Result()

        /**
         * The [Login] cannot be saved.
         */
        sealed class Error : Result() {
            /**
             * The passed [Login] had an empty password field, and so cannot be saved.
             */
            object EmptyPassword : Error()

            /**
             * Something went wrong in GeckoView. We have no way to handle this type of error. See
             * [exception] for details.
             */
            data class GeckoError(val exception: Exception) : Error()
        }
    }

    /**
     * Checks whether or not [login] can be persisted.
     *
     * Note that this method is not thread safe.
     *
     * @returns a [LoginValidationDelegate.Result], detailing whether [login] can be saved as a new
     * value, used to update an existing one, or an error occured.
     */
    fun validateCanPersist(login: Login): Deferred<Result>
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
     * Given a [domain], returns a [GeckoResult] of the matching [LoginEntry]s found in
     * [loginStorage].
     *
     * This is called when the engine believes a field should be autofilled.
     */
    fun onLoginFetch(domain: String): Deferred<List<Login>>

    /**
     * Called when a [login] should be saved or updated.
     */
    fun onLoginSave(login: Login)
}
