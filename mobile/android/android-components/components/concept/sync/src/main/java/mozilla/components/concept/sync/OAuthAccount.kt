/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.sync

import kotlinx.coroutines.Deferred

/**
 * An auth-related exception type, for use with [AuthException].
 */
enum class AuthExceptionType(val msg: String) {
    KEY_INFO("Missing key info"),
    NO_TOKEN("Missing access token"),
    UNAUTHORIZED("Unauthorized")
}

/**
 * An exception which may happen while obtaining auth information using [OAuthAccount].
 */
class AuthException(type: AuthExceptionType, cause: Exception? = null) : Throwable(type.msg, cause)

/**
 * Facilitates testing consumers of FirefoxAccount.
 */
@SuppressWarnings("TooManyFunctions")
interface OAuthAccount : AutoCloseable {
    fun beginOAuthFlowAsync(scopes: Set<String>): Deferred<String?>
    fun beginPairingFlowAsync(pairingUrl: String, scopes: Set<String>): Deferred<String?>
    fun getProfileAsync(ignoreCache: Boolean): Deferred<Profile?>
    fun getProfileAsync(): Deferred<Profile?>
    fun completeOAuthFlowAsync(code: String, state: String): Deferred<Boolean>
    fun getAccessTokenAsync(singleScope: String): Deferred<AccessTokenInfo?>
    fun checkAuthorizationStatusAsync(singleScope: String): Deferred<Boolean?>
    fun getTokenServerEndpointURL(): String
    fun registerPersistenceCallback(callback: StatePersistenceCallback)
    fun migrateFromSessionTokenAsync(sessionToken: String, kSync: String, kXCS: String): Deferred<Boolean>
    fun deviceConstellation(): DeviceConstellation
    fun disconnectAsync(): Deferred<Boolean>
    fun toJSONString(): String
}

/**
 * Describes a delegate object that is used by [OAuthAccount] to persist its internal state as it changes.
 */
interface StatePersistenceCallback {
    /**
     * @param data Account state representation as a string (e.g. as json).
     */
    fun persist(data: String)
}

/**
 * Observer interface which lets its users monitor account state changes and major events.
 */
interface AccountObserver {
    /**
     * Account just got logged out.
     */
    fun onLoggedOut() = Unit

    /**
     * Account was successfully authenticated.
     * @param account An authenticated instance of a [OAuthAccount].
     * @param newAccount True if this is a new account that was authenticated.
     */
    fun onAuthenticated(account: OAuthAccount, newAccount: Boolean) = Unit

    /**
     * Account's profile is now available.
     * @param profile A fresh version of account's [Profile].
     */
    fun onProfileUpdated(profile: Profile) = Unit

    /**
     * Account needs to be re-authenticated (e.g. due to a password change).
     */
    fun onAuthenticationProblems() = Unit
}

data class Avatar(
    val url: String,
    val isDefault: Boolean
)

data class Profile(
    val uid: String?,
    val email: String?,
    val avatar: Avatar?,
    val displayName: String?
)

/**
 * Scoped key data.
 *
 * @property kid The JWK key identifier.
 * @property k The JWK key data.
 */
data class OAuthScopedKey(
    val kty: String,
    val scope: String,
    val kid: String,
    val k: String
)

/**
 * The result of authentication with FxA via an OAuth flow.
 *
 * @property token The access token produced by the flow.
 * @property key An OAuthScopedKey if present.
 * @property expiresAt The expiry date timestamp of this token since unix epoch (in seconds).
 */
data class AccessTokenInfo(
    val scope: String,
    val token: String,
    val key: OAuthScopedKey?,
    val expiresAt: Long
)
