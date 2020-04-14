/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.logins

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.async
import mozilla.appservices.logins.InvalidLoginReason
import mozilla.appservices.logins.InvalidRecordException
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginValidationDelegate
import mozilla.components.concept.storage.LoginValidationDelegate.Result
import mozilla.components.concept.storage.LoginsStorage

/**
 * A delegate that will check against [storage] to see if a given Login can be persisted, and return
 * information about why it can or cannot.
 */
class DefaultLoginValidationDelegate(
    private val storage: Lazy<LoginsStorage>,
    private val scope: CoroutineScope = CoroutineScope(IO)
) : LoginValidationDelegate {

    /**
     * Compares a [Login] to a passed in list of potential dupes [Login] or queries underlying
     * storage for potential dupes list of [Login] to determine if it should be updated or created.
     */
    override fun shouldUpdateOrCreateAsync(
        newLogin: Login,
        potentialDupes: List<Login>?
    ): Deferred<Result> {
        return scope.async {
            val potentialDupesList =
                potentialDupes ?: storage.value.getPotentialDupesIgnoringUsername(newLogin)
            val foundLogin = if (potentialDupesList.isEmpty()) {
                // If list is empty, no records match -> create
                null
            } else {
                // Matching guid, non-blank matching username -> update
                potentialDupesList.find { it.guid == newLogin.guid && it.username == newLogin.username }
                // Matching guid, blank username -> update
                    ?: potentialDupesList.find { it.guid == newLogin.guid && it.username.isEmpty() }
                    // Matching username -> update
                    ?: potentialDupesList.find { it.username == newLogin.username }
                    // Non matching guid, blank username -> update
                    ?: potentialDupesList.find { it.username.isEmpty() }
                    // else create
            }
            if (foundLogin == null) Result.CanBeCreated else Result.CanBeUpdated(foundLogin)
        }
    }

    override fun getPotentialDupesIgnoringUsernameAsync(newLogin: Login): Deferred<List<Login>> {
        return scope.async {
            storage.value.getPotentialDupesIgnoringUsername(newLogin)
        }
    }

    /**
     * Queries underlying storage for a new [Login] to determine if should be updated or created,
     * and can be merged and saved, or any error states.
     *
     * @return a [Result] detailing whether we should create or update (and the merged login to update),
     * else any error states
     */
    @Suppress("ComplexMethod")
    fun ensureValidAsync(login: Login): Deferred<Result> {
        return scope.async {
            try {
                var result = shouldUpdateOrCreateAsync(login).await()
                val loginToCheck = when (result) {
                    // If we are updating, let's validate the merged login
                    is Result.CanBeUpdated -> result.foundLogin.mergeWithLogin(login)
                    is Result.CanBeCreated -> login
                    else -> login
                }
                storage.value.ensureValid(loginToCheck)
                // Return the merged login to update to avoid doing this merge twice
                if (result is Result.CanBeUpdated) {
                    result = Result.CanBeUpdated(loginToCheck)
                }
                result
            } catch (e: InvalidRecordException) {
                when (e.reason) {
                    InvalidLoginReason.DUPLICATE_LOGIN -> Result.Error.Duplicate
                    InvalidLoginReason.EMPTY_PASSWORD -> Result.Error.EmptyPassword
                    InvalidLoginReason.EMPTY_ORIGIN -> Result.Error.GeckoError(e)
                    InvalidLoginReason.BOTH_TARGETS -> Result.Error.GeckoError(e)
                    InvalidLoginReason.NO_TARGET -> Result.Error.GeckoError(e)
                    InvalidLoginReason.ILLEGAL_FIELD_VALUE -> Result.Error.GeckoError(e)
                }
            }
        }
    }

    companion object {
        /**
         * Will use values from [login] if they are 1) non-null and 2) non-empty.  Otherwise, will fall
         * back to values from [this].
         */
        fun Login.mergeWithLogin(login: Login): Login {
            infix fun String?.orUseExisting(other: String?) =
                if (this?.isNotEmpty() == true) this else other

            infix fun String?.orUseExisting(other: String) =
                if (this?.isNotEmpty() == true) this else other

            val origin = login.origin orUseExisting origin
            val username = login.username orUseExisting username
            val password = login.password orUseExisting password
            val httpRealm = login.httpRealm orUseExisting httpRealm
            val formActionOrigin = login.formActionOrigin orUseExisting formActionOrigin

            return copy(
                origin = origin,
                username = username,
                password = password,
                httpRealm = httpRealm,
                formActionOrigin = formActionOrigin
            )
        }
    }
}
