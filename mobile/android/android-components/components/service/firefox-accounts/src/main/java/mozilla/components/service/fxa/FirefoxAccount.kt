/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

/**
 * FirefoxAccount represents the authentication state of a client.
 *
 * @param <T> The type of the value delivered via the FxaResult.
 */
class FirefoxAccount(override var rawPointer: RawFxAccount?) : RustObject<RawFxAccount>() {

    constructor(config: Config, clientId: String, redirectUri: String): this(null) {
        this.rawPointer = safeSync { e ->
            FxaClient.INSTANCE.fxa_new(config.consumePointer(), clientId, redirectUri, e)
        }
    }

    override fun destroy(p: RawFxAccount) {
        safeSync { FxaClient.INSTANCE.fxa_free(p) }
    }

    /**
     * Constructs a URL used to begin the OAuth flow for the requested scopes and keys.
     *
     * @param scopes List of OAuth scopes for which the client wants access
     * @param wantsKeys Fetch keys for end-to-end encryption of data from Mozilla-hosted services
     * @return FxaResult<String> that resolves to the flow URL when complete
     */
    fun beginOAuthFlow(scopes: Array<String>, wantsKeys: Boolean): FxaResult<String> {
        return safeAsync { e ->
            val scope = scopes.joinToString(" ")
            val p = FxaClient.INSTANCE.fxa_begin_oauth_flow(validPointer(), scope, wantsKeys, e)
            getAndConsumeString(p) ?: ""
        }
    }

    fun beginPairingFlow(pairingUrl: String, scopes: Array<String>): FxaResult<String> {
        return safeAsync { e ->
            val scope = scopes.joinToString(" ")
            val p = FxaClient.INSTANCE.fxa_begin_pairing_flow(validPointer(), pairingUrl, scope, e)
            getAndConsumeString(p) ?: ""
        }
    }

    /**
     * Fetches the profile object for the current client either from the existing cached account,
     * or from the server (requires the client to have access to the profile scope).
     *
     * @param ignoreCache Fetch the profile information directly from the server
     * @return FxaResult<[Profile]> representing the user's basic profile info
     */
    fun getProfile(ignoreCache: Boolean): FxaResult<Profile> {
        return safeAsync { e ->
            val p = FxaClient.INSTANCE.fxa_profile(validPointer(), ignoreCache, e)
            Profile(p)
        }
    }

    /**
     * Convenience method to fetch the profile from a cached account by default, but fall back
     * to retrieval from the server.
     *
     * @return FxaResult<[Profile]> representing the user's basic profile info
     */
    fun getProfile(): FxaResult<Profile> {
        return getProfile(false)
    }

    /**
     * Fetches the token server endpoint, for authentication using the SAML bearer flow.
     */
    fun getTokenServerEndpointURL(): String? {
        return safeSync { e ->
            val p = FxaClient.INSTANCE.fxa_get_token_server_endpoint_url(validPointer(), e)
            getAndConsumeString(p)
        }
    }

    /**
     * Fetches keys for encryption/decryption of Firefox Sync data.
     */
    fun getSyncKeys(): SyncKeys {
        return safeSync { e ->
            val p = FxaClient.INSTANCE.fxa_get_sync_keys(validPointer(), e)
            SyncKeys(p)
        }
    }

    /**
     * Authenticates the current account using the code and state parameters fetched from the
     * redirect URL reached after completing the sign in flow triggered by [beginOAuthFlow].
     *
     * Modifies the FirefoxAccount state.
     */
    fun completeOAuthFlow(code: String, state: String): FxaResult<OAuthInfo> {
        return safeAsync { e ->
            val p = FxaClient.INSTANCE.fxa_complete_oauth_flow(validPointer(), code, state, e)
            OAuthInfo(p)
        }
    }

    /**
     * Fetches a new access token for the desired scopes using an internally stored refresh token.
     *
     * @param scopes List of OAuth scopes for which the client wants access
     * @return FxaResult<[OAuthInfo]> that stores the token, along with its scopes and keys when complete
     * @throws FxaException.Unauthorized if the token could not be retrieved (eg. expired refresh token)
     */
    fun getOAuthToken(scopes: Array<String>): FxaResult<OAuthInfo> {
        return safeAsync { e ->
            val scope = scopes.joinToString(" ")
            val p = FxaClient.INSTANCE.fxa_get_oauth_token(validPointer(), scope, e)
                    ?: throw FxaException.Unauthorized("Restart OAuth flow")
            OAuthInfo(p)
        }
    }

    /**
     * Saves the current account's authentication state as a JSON string, for persistence in
     * the Android KeyStore/shared preferences. The authentication state can be restored using
     * [FirefoxAccount.fromJSONString].
     *
     * @return String containing the authentication details in JSON format
     */
    fun toJSONString(): String? {
        return safeSync { e ->
            val p = FxaClient.INSTANCE.fxa_to_json(validPointer(), e)
            getAndConsumeString(p)
        }
    }

    companion object {
        fun from(
            config: Config,
            clientId: String,
            redirectUri: String,
            webChannelResponse: String
        ): FxaResult<FirefoxAccount> {
            return RustObject.safeAsync { e ->
                val p = FxaClient.INSTANCE.fxa_from_credentials(config.consumePointer(),
                        clientId, redirectUri, webChannelResponse, e)
                FirefoxAccount(p)
            }
        }

        /**
         * Restores the account's authentication state from a JSON string produced by
         * [FirefoxAccount.toJSONString].
         *
         * @return FxaResult<[FirefoxAccount]> representing the authentication state
         */
        fun fromJSONString(json: String): FxaResult<FirefoxAccount> {
            return RustObject.safeAsync { e ->
                val p = FxaClient.INSTANCE.fxa_from_json(json, e)
                FirefoxAccount(p)
            }
        }
    }
}
