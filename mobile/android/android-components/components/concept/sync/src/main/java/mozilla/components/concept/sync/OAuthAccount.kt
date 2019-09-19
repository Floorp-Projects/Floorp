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
 * An object that represents a login flow initiated by [OAuthAccount].
 * @property state OAuth state parameter, identifying a specific authentication flow.
 * This string is randomly generated during [OAuthAccount.beginOAuthFlowAsync] and [OAuthAccount.beginPairingFlowAsync].
 * @property url Url which needs to be loaded to go through the authentication flow identified by [state].
 */
data class AuthFlowUrl(val state: String, val url: String)

/**
 * Facilitates testing consumers of FirefoxAccount.
 */
@SuppressWarnings("TooManyFunctions")
interface OAuthAccount : AutoCloseable {
    fun beginOAuthFlowAsync(scopes: Set<String>): Deferred<AuthFlowUrl?>
    fun beginPairingFlowAsync(pairingUrl: String, scopes: Set<String>): Deferred<AuthFlowUrl?>
    fun getCurrentDeviceId(): String?
    fun getSessionToken(): String?
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

sealed class AuthType {
    /**
     * Account restored from hydrated state on disk.
     */
    object Existing : AuthType()

    /**
     * Account created in response to a sign-in.
     */
    object Signin : AuthType()

    /**
     * Account created in response to a sign-up.
     */
    object Signup : AuthType()

    /**
     * Account created via pairing (similar to sign-in, but without requiring credentials).
     */
    object Pairing : AuthType()

    /**
     * Account was created for an unknown external reason, hopefully identified by [action].
     */
    data class OtherExternal(val action: String?) : AuthType()

    /**
     * Account created via a shared account state from another app.
     */
    object Shared : AuthType()

    /**
     * Existing account was recovered from an authentication problem.
     */
    object Recovered : AuthType()
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
     *
     * @param account An authenticated instance of a [OAuthAccount].
     * @param authType Describes what kind of authentication event caused this invocation.
     */
    fun onAuthenticated(account: OAuthAccount, authType: AuthType) = Unit

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
