package mozilla.components.concept.sync

import kotlinx.coroutines.Deferred

/**
 * An auth-related exception type, for use with [AuthException].
 */
enum class AuthExceptionType(val msg: String) {
    KEY_INFO("Missing key info")
}

/**
 * An exception which may happen while obtaining auth information using [OAuthAccount].
 */
class AuthException(type: AuthExceptionType) : java.lang.Exception(type.msg)

/**
 * Facilitates testing consumers of FirefoxAccount.
 */
interface OAuthAccount : AutoCloseable {
    fun beginOAuthFlow(scopes: Array<String>, wantsKeys: Boolean): Deferred<String>
    fun beginPairingFlow(pairingUrl: String, scopes: Array<String>): Deferred<String>
    fun getProfile(ignoreCache: Boolean): Deferred<Profile>
    fun getProfile(): Deferred<Profile>
    fun completeOAuthFlow(code: String, state: String): Deferred<Unit>
    fun getAccessToken(singleScope: String): Deferred<AccessTokenInfo>
    fun getTokenServerEndpointURL(): String
    fun toJSONString(): String

    suspend fun authInfo(singleScope: String): AuthInfo {
        val tokenServerURL = this.getTokenServerEndpointURL()
        val tokenInfo = this.getAccessToken(singleScope).await()
        val keyInfo = tokenInfo.key ?: throw AuthException(AuthExceptionType.KEY_INFO)

        return AuthInfo(
                kid = keyInfo.kid,
                fxaAccessToken = tokenInfo.token,
                syncKey = keyInfo.k,
                tokenServerUrl = tokenServerURL
        )
    }
}

/**
 * A Firefox Sync friendly auth object which can be obtained from [OAuthAccount].
 */
data class AuthInfo(
    val kid: String,
    val fxaAccessToken: String,
    val syncKey: String,
    val tokenServerUrl: String
)

/**
 * Observer interface which lets its users monitor account state changes and major events.
 */
interface AccountObserver {
    /**
     * Account just got logged out.
     */
    fun onLoggedOut()

    /**
     * Account was successfully authenticated.
     * @param account An authenticated instance of a [OAuthAccount].
     */
    fun onAuthenticated(account: OAuthAccount)

    /**
     * Account's profile is now available.
     * @param profile A fresh version of account's [Profile].
     */
    fun onProfileUpdated(profile: Profile)

    /**
     * Account manager encountered an error. Inspect [error] for details.
     * @param error A specific error encountered.
     */
    fun onError(error: Exception)
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
