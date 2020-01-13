/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.storage

import kotlinx.coroutines.Deferred

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
    val password: String
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
    fun onLoginFetch(domain: String): List<Login>
    /**
     * Called when a [login] should be saved or updated.
     */
    fun onLoginSave(login: Login)
}
