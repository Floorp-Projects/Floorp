/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.sync

import kotlinx.coroutines.Deferred
import org.json.JSONObject

/**
 * An object that represents a login flow initiated by [OAuthAccount].
 * @property state OAuth state parameter, identifying a specific authentication flow.
 * This string is randomly generated during [OAuthAccount.beginOAuthFlow] and [OAuthAccount.beginPairingFlow].
 * @property url Url which needs to be loaded to go through the authentication flow identified by [state].
 */
data class AuthFlowUrl(val state: String, val url: String)

/**
 * Represents a specific type of an "in-flight" migration state that could result from intermittent
 * issues during [OAuthAccount.migrateFromAccount].
 */
enum class InFlightMigrationState(val reuseSessionToken: Boolean) {
    /**
     * "Copy" in-flight migration present. Can retry migration via [OAuthAccount.retryMigrateFromSessionToken].
     */
    COPY_SESSION_TOKEN(false),

    /**
     * "Reuse" in-flight migration present. Can retry migration via [OAuthAccount.retryMigrateFromSessionToken].
     */
    REUSE_SESSION_TOKEN(true),
}

/**
 * Data structure describing FxA and Sync credentials necessary to sign-in into an FxA account.
 */
data class MigratingAccountInfo(
    val sessionToken: String,
    val kSync: String,
    val kXCS: String,
)

/**
 * Facilitates testing consumers of FirefoxAccount.
 */
interface OAuthAccount : AutoCloseable {

    /**
     * Constructs a URL used to begin the OAuth flow for the requested scopes and keys.
     *
     * @param scopes List of OAuth scopes for which the client wants access
     * @param entryPoint The UI entryPoint used to start this flow. An arbitrary
     * string which is recorded in telemetry by the server to help analyze the
     * most effective touchpoints
     * @return [AuthFlowUrl] if available, `null` in case of a failure
     */
    suspend fun beginOAuthFlow(
        scopes: Set<String>,
        entryPoint: String = "android-components",
    ): AuthFlowUrl?

    /**
     * Constructs a URL used to begin the pairing flow for the requested scopes and pairingUrl.
     *
     * @param pairingUrl URL string for pairing
     * @param scopes List of OAuth scopes for which the client wants access
     * @param entryPoint The UI entryPoint used to start this flow. An arbitrary
     * string which is recorded in telemetry by the server to help analyze the
     * most effective touchpoints
     * @return [AuthFlowUrl] if available, `null` in case of a failure
     */
    suspend fun beginPairingFlow(
        pairingUrl: String,
        scopes: Set<String>,
        entryPoint: String = "android-components",
    ): AuthFlowUrl?

    /**
     * Returns current FxA Device ID for an authenticated account.
     *
     * @return Current device's FxA ID, if available. `null` otherwise.
     */
    fun getCurrentDeviceId(): String?

    /**
     * Returns session token for an authenticated account.
     *
     * @return Current account's session token, if available. `null` otherwise.
     */
    fun getSessionToken(): String?

    /**
     * Fetches the profile object for the current client either from the existing cached state
     * or from the server (requires the client to have access to the profile scope).
     *
     * @param ignoreCache Fetch the profile information directly from the server
     * @return Profile (optional, if successfully retrieved) representing the user's basic profile info
     */
    suspend fun getProfile(ignoreCache: Boolean = false): Profile?

    /**
     * Authenticates the current account using the [code] and [state] parameters obtained via the
     * OAuth flow initiated by [beginOAuthFlow].
     *
     * Modifies the FirefoxAccount state.
     * @param code OAuth code string
     * @param state state token string
     * @return Deferred boolean representing success or failure
     */
    suspend fun completeOAuthFlow(code: String, state: String): Boolean

    /**
     * Tries to fetch an access token for the given scope.
     *
     * @param singleScope Single OAuth scope (no spaces) for which the client wants access
     * @return [AccessTokenInfo] that stores the token, along with its scope, key and
     *                           expiration timestamp (in seconds) since epoch when complete
     */
    suspend fun getAccessToken(singleScope: String): AccessTokenInfo?

    /**
     * Call this whenever an authentication error was encountered while using an access token
     * issued by [getAccessToken].
     */
    fun authErrorDetected()

    /**
     * This method should be called when a request made with an OAuth token failed with an
     * authentication error. It will re-build cached state and perform a connectivity check.
     *
     * In time, fxalib will grow a similar method, at which point we'll just relay to it.
     * See https://github.com/mozilla/application-services/issues/1263
     *
     * @param singleScope An oauth scope for which to check authorization state.
     * @return An optional [Boolean] flag indicating if we're connected, or need to go through
     * re-authentication. A null result means we were not able to determine state at this time.
     */
    suspend fun checkAuthorizationStatus(singleScope: String): Boolean?

    /**
     * Fetches the token server endpoint, for authentication using the SAML bearer flow.
     *
     * @return Token server endpoint URL string, `null` if it couldn't be obtained.
     */
    suspend fun getTokenServerEndpointURL(): String?

    /**
     * Get the pairing URL to navigate to on the Authority side (typically a computer).
     *
     * @return The URL to show the pairing user
     */
    fun getPairingAuthorityURL(): String

    /**
     * Registers a callback for when the account state gets persisted
     *
     * @param callback the account state persistence callback
     */
    fun registerPersistenceCallback(callback: StatePersistenceCallback)

    /**
     * Attempts to migrate from an existing session token without user input.
     * Passed-in session token will be reused.
     *
     * @param authInfo Auth info necessary for signing in
     * @param reuseSessionToken Whether or not session token should be reused; reusing session token
     * means that FxA device record will be inherited
     * @return JSON object with the result of the migration or 'null' if it failed.
     * For up-to-date schema, see underlying implementation in
     * https://github.com/mozilla/application-services/blob/v0.49.0/components/fxa-client/src/migrator.rs#L10
     * At the moment, it's just "{total_duration: long}".
     */
    suspend fun migrateFromAccount(authInfo: MigratingAccountInfo, reuseSessionToken: Boolean): JSONObject?

    /**
     * Checks if there's a migration in-flight. An in-flight migration means that we've tried to migrate
     * via [migrateFromAccount], and failed for intermittent (e.g. network) reasons. When an in-flight
     * migration is present, we can retry using [retryMigrateFromSessionToken].
     * @return InFlightMigrationState indicating specific migration state. [null] if not in a migration state.
     */
    fun isInMigrationState(): InFlightMigrationState?

    /**
     * Retries an in-flight migration attempt.
     * @return JSON object with the result of the retry attempt or 'null' if it failed.
     * For up-to-date schema, see underlying implementation in https://github.com/mozilla/application-services/blob/v0.49.0/components/fxa-client/src/migrator.rs#L10
     * At the moment, it's just "{total_duration: long}".
     */
    suspend fun retryMigrateFromSessionToken(): JSONObject?

    /**
     * Returns the device constellation for the current account
     *
     * @return Device constellation for the current account
     */
    fun deviceConstellation(): DeviceConstellation

    /**
     * Reset internal account state and destroy current device record.
     * Use this when device record is no longer relevant, e.g. while logging out. On success, other
     * devices will no longer see the current device in their device lists.
     *
     * @return A [Deferred] that will be resolved with a success flag once operation is complete.
     * Failure indicates that we may have failed to destroy current device record. Nothing to do for
     * the consumer; device record will be cleaned up eventually via TTL.
     */
    suspend fun disconnect(): Boolean

    /**
     * Serializes the current account's authentication state as a JSON string, for persistence in
     * the Android KeyStore/shared preferences. The authentication state can be restored using
     * [FirefoxAccount.fromJSONString].
     *
     * @return String containing the authentication details in JSON format
     */
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
     * Account created via a shared account state from another app via the copy token flow.
     */
    object MigratedCopy : AuthType()

    /**
     * Account created via a shared account state from another app via the reuse token flow.
     */
    object MigratedReuse : AuthType()

    /**
     * Existing account was recovered from an authentication problem.
     */
    object Recovered : AuthType()
}

/**
 * Different types of errors that may be encountered during authorization and migration flows.
 * Intermittent network problems are the most common reason for these errors.
 */
enum class AuthFlowError {
    /**
     * Couldn't begin authorization, i.e. failed to obtain an authorization URL.
     */
    FailedToBeginAuth,

    /**
     * Couldn't complete authorization after user entered valid credentials/paired correctly.
     */
    FailedToCompleteAuth,

    /**
     * Unrecoverable error during account migration.
     */
    FailedToMigrate,
}

/**
 * Observer interface which lets its users monitor account state changes and major events.
 * (XXX - there's some tension between this and the
 * mozilla.components.concept.sync.AccountEvent we should resolve!)
 */
interface AccountObserver {
    /**
     * Account state has been resolved and can now be queried.
     *
     * @param authenticatedAccount Currently resolved authenticated account, if any.
     */
    fun onReady(authenticatedAccount: OAuthAccount?) = Unit

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

    /**
     * Encountered an error during an authentication or migration flow.
     * @param error Exact error encountered.
     */
    fun onFlowError(error: AuthFlowError) = Unit
}

data class Avatar(
    val url: String,
    val isDefault: Boolean,
)

data class Profile(
    val uid: String?,
    val email: String?,
    val avatar: Avatar?,
    val displayName: String?,
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
    val k: String,
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
    val expiresAt: Long,
)
