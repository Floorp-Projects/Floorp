/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.logins

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginStorageDelegate
import mozilla.components.concept.storage.LoginsStorage
import mozilla.components.support.base.log.logger.Logger

/**
 * A type of persistence operation, either 'create' or 'update'.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
enum class Operation { CREATE, UPDATE }

/**
 * [LoginStorageDelegate] implementation.
 *
 * An abstraction that handles the persistence and retrieval of [LoginEntry]s so that Gecko doesn't
 * have to.
 *
 * In order to use this class, attach it to the active [GeckoRuntime] as its `loginStorageDelegate`.
 * It is not designed to work with other engines.
 *
 * This class is part of a complex flow integrating Gecko and Application Services code, which is
 * described here:
 *
 * - GV finds something on a page that it believes could be autofilled
 * - GV calls [onLoginFetch]
 *   - We retrieve all [Login]s with matching domains (if any) from [loginStorage]
 *   - We return these [Login]s to GV
 * - GV autofills one of the returned [Login]s into the page
 *   - GV calls [onLoginUsed] to let us know which [Login] was used
 * - User submits their credentials
 * - GV detects something that looks like a login submission
 *   - ([GeckoLoginStorageDelegate] is not involved with this step) A prompt is shown to the user,
 *   who decides whether or not to save the [Login]
 *     - If the user declines, break
 *   - GV calls [onLoginSave] with a [Login]
 *     - If this [Login] was autofilled, it is updated with new information but retains the same
 *     [Login.guid]
 *     - If this is a new [Login], its [Login.guid] will be an empty string
 *   - We [CREATE] or [UPDATE] the [Login] in [loginStorage]
 *     - If [CREATE], [loginStorage] generates a new guid for it
 */
class GeckoLoginStorageDelegate(
    private val loginStorage: LoginsStorage,
    private val isAutofillEnabled: () -> Boolean = { false },
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO)
) : LoginStorageDelegate {

    private val logger = Logger("GeckoLoginStorageDelegate")

    override fun onLoginUsed(login: Login) {
        val guid = login.guid
        // If the guid is null, we have no way of associating the login with any record in the DB
        if (guid == null || guid.isEmpty()) return
        scope.launch {
            loginStorage.touch(guid)
        }
    }

    override fun onLoginFetch(domain: String): Deferred<List<Login>> {
        if (!isAutofillEnabled()) return CompletableDeferred(listOf())
        return scope.async {
            loginStorage.getByBaseDomain(domain)
        }
    }

    @Synchronized
    override fun onLoginSave(login: Login) {
        scope.launch {
            val existingLogin = login.guid?.let { loginStorage.get(it) }

            when (getPersistenceOperation(login, existingLogin)) {
                Operation.UPDATE -> {
                    existingLogin?.let { loginStorage.update(it.mergeWithLogin(login)) }
                }
                Operation.CREATE -> {
                    // If an existing Login was autofilled, we want to clear its guid to
                    // avoid updating its record
                    loginStorage.add(login.copy(guid = null))
                }
            }
        }
    }

    /**
     * Returns whether an existing login record should be UPDATED or a new one [CREATE]d, based
     * on the saved [Login] and new [Login].
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun getPersistenceOperation(newLogin: Login, savedLogin: Login?): Operation = when {
        newLogin.guid.isNullOrEmpty() || savedLogin == null -> Operation.CREATE
        newLogin.guid != savedLogin.guid -> {
            logger.debug(
                "getPersistenceOperation called with a non-null `savedLogin` with" +
                        " a guid that does not match `newLogin`. This is unexpected. Falling back to create " +
                        "new login."
            )
            Operation.CREATE
        }
        // This means a password was saved for this site with a blank username. Update that record
        savedLogin.username.isEmpty() -> Operation.UPDATE
        newLogin.username != savedLogin.username -> Operation.CREATE
        else -> Operation.UPDATE
    }
}

/**
 * Will use values from [this] if they are 1) non-null and 2) non-empty.  Otherwise, will fall
 * back to values from [this].
 */
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
fun Login.mergeWithLogin(login: Login): Login {
    infix fun String?.orUseExisting(other: String?) =
        if (this?.isNotEmpty() == true) this else other

    infix fun String?.orUseExisting(other: String) = if (this?.isNotEmpty() == true) this else other

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

/**
 * Converts an Android Components [Login] to an Application Services [ServerPassword]
 */
fun Login.toServerPassword() = ServerPassword(
    // Underlying Rust code will generate a new GUID if `guid` is missing.
    id = guid ?: "",
    username = username,
    password = password,
    hostname = origin,
    formSubmitURL = formActionOrigin,
    httpRealm = httpRealm,
    timesUsed = timesUsed,
    timeCreated = timeCreated,
    timeLastUsed = timeLastUsed,
    timePasswordChanged = timePasswordChanged,
    // usernameField & passwordField are allowed to be empty when
    // information is not available
    usernameField = usernameField ?: "",
    passwordField = passwordField ?: ""
)

/**
 * Converts an Application Services [ServerPassword] to an Android Components [Login]
 */
fun ServerPassword.toLogin() = Login(
    guid = id,
    origin = hostname,
    formActionOrigin = formSubmitURL,
    httpRealm = httpRealm,
    username = username,
    password = password,
    timesUsed = timesUsed,
    timeCreated = timeCreated,
    timeLastUsed = timeLastUsed,
    timePasswordChanged = timePasswordChanged,
    usernameField = usernameField,
    passwordField = passwordField
)
