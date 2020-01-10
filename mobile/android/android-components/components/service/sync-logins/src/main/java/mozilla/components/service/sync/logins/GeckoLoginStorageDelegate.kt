/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.logins

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginStorageDelegate
import mozilla.components.lib.dataprotect.SecureAbove22Preferences
import mozilla.components.support.base.log.logger.Logger

/**
 * A type of persistence operation, either 'create' or 'update'.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
enum class Operation { CREATE, UPDATE }

internal const val PASSWORDS_KEY = "passwords"

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
    private val loginStorage: AsyncLoginsStorage,
    keyStore: SecureAbove22Preferences,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO)
) : LoginStorageDelegate {

    private val password = { scope.async { keyStore.getString(PASSWORDS_KEY)!! } }
    private val logger = Logger("GeckoLoginStorageDelegate")

    override fun onLoginUsed(login: Login) {
        val guid = login.guid
        // If the guid is null, we have no way of associating the login with any record in the DB
        if (guid == null || guid.isEmpty()) return
        scope.launch {
            loginStorage.withUnlocked(password) {
                loginStorage.touch(guid).await()
            }
        }
    }

    override fun onLoginFetch(domain: String): List<Login> {
        return runBlocking {
            // GV expects a synchronous response. Blocking here hasn't caused problems during
            // testing, but we should add telemetry to verify. See #5531
            loginStorage.withUnlocked(password) {
                loginStorage.getByBaseDomain(domain).await()
                    .map { it.toLogin() }
            }
        }
    }

    @Synchronized
    override fun onLoginSave(login: Login) {
        scope.launch {
            loginStorage.withUnlocked(password) {
                val existingLogin = login.guid?.let { loginStorage.get(it) }?.await()

                when (getPersistenceOperation(login, existingLogin)) {
                    Operation.UPDATE -> {
                        existingLogin?.let { loginStorage.update(it.mergeWithLogin(login)).await() }
                    }
                    Operation.CREATE -> {
                        // If an existing Login was autofilled, we want to clear its guid to
                        // avoid updating its record
                        loginStorage.add(login.copy(guid = "").toServerPassword()).await()
                    }
                }
            }
        }
    }

    /**
     * Returns whether an existing login record should be [UPDATE]d or a new one [CREATE]d, based
     * on the saved [ServerPassword] and new [Login].
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun getPersistenceOperation(newLogin: Login, savedLogin: ServerPassword?): Operation = when {
        newLogin.guid.isNullOrEmpty() || savedLogin == null -> Operation.CREATE
        newLogin.guid != savedLogin.id -> {
            logger.debug("getPersistenceOperation called with a non-null `savedLogin` with" +
                " a guid that does not match `newLogin`. This is unexpected. Falling back to create " +
                "new login.")
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
fun ServerPassword.mergeWithLogin(login: Login): ServerPassword {
    infix fun String?.orUseExisting(other: String?) = if (this?.isNotEmpty() == true) this else other
    infix fun String?.orUseExisting(other: String) = if (this?.isNotEmpty() == true) this else other

    val hostname = login.origin orUseExisting hostname
    val username = login.username orUseExisting username
    val password = login.password orUseExisting password
    val httpRealm = login.httpRealm orUseExisting httpRealm
    val formSubmitUrl = login.formActionOrigin orUseExisting formSubmitURL

    return copy(
        hostname = hostname,
        username = username,
        password = password,
        httpRealm = httpRealm,
        formSubmitURL = formSubmitUrl
    )
}

/**
 * Converts an Android Components [Login] to an Application Services [ServerPassword]
 */
fun Login.toServerPassword() = ServerPassword(
    // Underlying Rust code will generate a new GUID
    id = "",
    username = username,
    password = password,
    hostname = origin,
    formSubmitURL = formActionOrigin,
    httpRealm = httpRealm,
    // usernameField & passwordField are allowed to be empty when
    // information is not available
    usernameField = "",
    passwordField = ""
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
    password = password
)
