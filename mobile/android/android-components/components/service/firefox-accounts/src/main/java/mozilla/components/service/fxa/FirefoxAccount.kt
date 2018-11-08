/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import kotlinx.coroutines.async
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Job
import kotlinx.coroutines.plus
import org.mozilla.fxaclient.internal.FirefoxAccount as InternalFxAcct

/**
 * FirefoxAccount represents the authentication state of a client.
 */
class FirefoxAccount internal constructor(private val inner: InternalFxAcct) : AutoCloseable {

    private val job = Job()
    private val scope = CoroutineScope(Dispatchers.IO) + job

    /**
     * Construct a FirefoxAccount from a [Config], a clientId, and a redirectUri.
     *
     * Note that it is not necessary to `close` the Config if this constructor is used (however
     * doing so will not cause an error).
     */
    constructor(config: Config, clientId: String, redirectUri: String)
            : this(InternalFxAcct(config.inner, clientId, redirectUri))

    override fun close() {
        job.cancel()
        inner.close()
    }

    /**
     * Constructs a URL used to begin the OAuth flow for the requested scopes and keys.
     *
     * @param scopes List of OAuth scopes for which the client wants access
     * @param wantsKeys Fetch keys for end-to-end encryption of data from Mozilla-hosted services
     * @return Deferred<String> that resolves to the flow URL when complete
     */
    fun beginOAuthFlow(scopes: Array<String>, wantsKeys: Boolean): Deferred<String> {
        return scope.async { inner.beginOAuthFlow(scopes, wantsKeys) }
    }

    fun beginPairingFlow(pairingUrl: String, scopes: Array<String>): Deferred<String> {
        return scope.async { inner.beginPairingFlow(pairingUrl, scopes) }
    }

    /**
     * Fetches the profile object for the current client either from the existing cached account,
     * or from the server (requires the client to have access to the profile scope).
     *
     * @param ignoreCache Fetch the profile information directly from the server
     * @return Deferred<[Profile]> representing the user's basic profile info
     */
    fun getProfile(ignoreCache: Boolean): Deferred<Profile> {
        return scope.async {
            val internalProfile = inner.getProfile(ignoreCache)
            Profile(
                    uid = internalProfile.uid,
                    email = internalProfile.email,
                    avatar = internalProfile.avatar,
                    displayName = internalProfile.displayName
            )
        }
    }

    /**
     * Convenience method to fetch the profile from a cached account by default, but fall back
     * to retrieval from the server.
     *
     * @return Deferred<[Profile]> representing the user's basic profile info
     */
    fun getProfile(): Deferred<Profile> = getProfile(false)

    /**
     * Fetches the token server endpoint, for authentication using the SAML bearer flow.
     */
    fun getTokenServerEndpointURL(): String = inner.getTokenServerEndpointURL()

    /**
     * Authenticates the current account using the code and state parameters fetched from the
     * redirect URL reached after completing the sign in flow triggered by [beginOAuthFlow].
     *
     * Modifies the FirefoxAccount state.
     */
    fun completeOAuthFlow(code: String, state: String): Deferred<OAuthInfo> {
        return scope.async { OAuthInfo.fromInternal(inner.completeOAuthFlow(code, state)) }
    }

    /**
     * Tries to fetch a cached access token for the given scope.
     *
     * If the token is close to expiration, we may refresh it.
     *
     * @param scopes List of OAuth scopes for which the client wants access
     * @return [OAuthInfo] that stores the token, along with its scopes and keys when complete
     */
    fun getCachedOAuthToken(scopes: Array<String>): Deferred<OAuthInfo?> {
        return scope.async {
            inner.getCachedOAuthToken(scopes)?.let { OAuthInfo.fromInternal(it) }
        }
    }

    /**
     * Saves the current account's authentication state as a JSON string, for persistence in
     * the Android KeyStore/shared preferences. The authentication state can be restored using
     * [FirefoxAccount.fromJSONString].
     *
     * @return String containing the authentication details in JSON format
     */
    fun toJSONString(): String = inner.toJSONString()

    companion object {
        /**
         * Restores the account's authentication state from a JSON string produced by
         * [FirefoxAccount.toJSONString].
         *
         * @return [FirefoxAccount] representing the authentication state
         */
        fun fromJSONString(json: String): FirefoxAccount {
            return FirefoxAccount(InternalFxAcct.fromJSONString(json))
        }
    }
}
