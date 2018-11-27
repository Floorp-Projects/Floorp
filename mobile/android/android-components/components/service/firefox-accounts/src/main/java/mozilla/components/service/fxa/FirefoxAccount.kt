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
import org.mozilla.fxaclient.internal.FxaException.Unauthorized as Unauthorized

/**
 * Facilitates testing consumers of FirefoxAccount.
 */
interface FirefoxAccountShaped {
    fun getAccessToken(singleScope: String): Deferred<AccessTokenInfo>
    fun getTokenServerEndpointURL(): String
}

/**
 * FirefoxAccount represents the authentication state of a client.
 */
class FirefoxAccount internal constructor(private val inner: InternalFxAcct) : AutoCloseable, FirefoxAccountShaped {

    private val job = Job()
    private val scope = CoroutineScope(Dispatchers.IO) + job

    /**
     * Construct a FirefoxAccount from a [Config], a clientId, and a redirectUri.
     *
     * Note that it is not necessary to `close` the Config if this constructor is used (however
     * doing so will not cause an error).
     */
    constructor(config: Config)
            : this(InternalFxAcct(config))

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
     * @throws Unauthorized We couldn't find any suitable access token to make that call.
     * The caller should then start the OAuth Flow again with the "profile" scope.
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
     * @throws Unauthorized We couldn't find any suitable access token to make that call.
     * The caller should then start the OAuth Flow again with the "profile" scope.
     */
    fun getProfile(): Deferred<Profile> = getProfile(false)

    /**
     * Fetches the token server endpoint, for authentication using the SAML bearer flow.
     */
    override fun getTokenServerEndpointURL(): String {
        return inner.getTokenServerEndpointURL()
    }

    /**
     * Authenticates the current account using the code and state parameters fetched from the
     * redirect URL reached after completing the sign in flow triggered by [beginOAuthFlow].
     *
     * Modifies the FirefoxAccount state.
     */
    fun completeOAuthFlow(code: String, state: String): Deferred<Unit> {
        return scope.async { inner.completeOAuthFlow(code, state) }
    }

    /**
     * Tries to fetch an access token for the given scope.
     *
     * @param scope Single OAuth scope (no spaces) for which the client wants access
     * @return [AccessTokenInfo] that stores the token, along with its scope, key and
     *                           expiration timestamp (in seconds) since epoch when complete
     * @throws Unauthorized We couldn't provide an access token for this scope.
     * The caller should then start the OAuth Flow again with the desired scope.
     */
    override fun getAccessToken(singleScope: String): Deferred<AccessTokenInfo> {
        return scope.async {
            inner.getAccessToken(singleScope).let { AccessTokenInfo.fromInternal(it) }
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
