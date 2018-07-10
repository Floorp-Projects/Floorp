/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

class FirefoxAccount(override var rawPointer: RawFxAccount?) : RustObject<RawFxAccount>() {

    constructor(config: Config, clientId: String): this(null) {
        this.rawPointer = safeSync { e -> FxaClient.INSTANCE.fxa_new(config.consumePointer(), clientId, e) }
    }

    override fun destroy(p: RawFxAccount) {
        safeSync { FxaClient.INSTANCE.fxa_free(p) }
    }

    fun beginOAuthFlow(redirectURI: String, scopes: Array<String>, wantsKeys: Boolean): FxaResult<String> {
        return safeAsync { e ->
            val scope = scopes.joinToString(" ")
            val p = FxaClient.INSTANCE.fxa_begin_oauth_flow(validPointer(), redirectURI, scope, wantsKeys, e)
            getAndConsumeString(p) ?: ""
        }
    }

    fun getProfile(ignoreCache: Boolean): FxaResult<Profile> {
        return safeAsync { e ->
            val p = FxaClient.INSTANCE.fxa_profile(validPointer(), ignoreCache, e)
            Profile(p)
        }
    }

    fun getProfile(): FxaResult<Profile> {
        return getProfile(false)
    }

    fun newAssertion(audience: String): String? {
        return safeSync { e ->
            val p = FxaClient.INSTANCE.fxa_assertion_new(this.validPointer(), audience, e)
            getAndConsumeString(p)
        }
    }

    fun getTokenServerEndpointURL(): String? {
        return safeSync { e ->
            val p = FxaClient.INSTANCE.fxa_get_token_server_endpoint_url(validPointer(), e)
            getAndConsumeString(p)
        }
    }

    fun getSyncKeys(): SyncKeys {
        return safeSync { e ->
            val p = FxaClient.INSTANCE.fxa_get_sync_keys(validPointer(), e)
            SyncKeys(p)
        }
    }

    fun completeOAuthFlow(code: String, state: String): FxaResult<OAuthInfo> {
        return safeAsync { e ->
            val p = FxaClient.INSTANCE.fxa_complete_oauth_flow(validPointer(), code, state, e)
            OAuthInfo(p)
        }
    }

    fun getOAuthToken(scopes: Array<String>): FxaResult<OAuthInfo> {
        return safeAsync { e ->
            val scope = scopes.joinToString(" ")
            val p = FxaClient.INSTANCE.fxa_get_oauth_token(validPointer(), scope, e)
                    ?: throw FxaException.Unauthorized("Restart OAuth flow")
            OAuthInfo(p)
        }
    }

    companion object {
        fun from(config: Config, clientId: String, webChannelResponse: String): FxaResult<FirefoxAccount> {
            return RustObject.safeAsync { e ->
                val p = FxaClient.INSTANCE.fxa_from_credentials(config.consumePointer(),
                        clientId, webChannelResponse, e)
                FirefoxAccount(p)
            }
        }

        fun fromJSONString(json: String): FxaResult<FirefoxAccount> {
            return RustObject.safeAsync { e ->
                val p = FxaClient.INSTANCE.fxa_from_json(json, e)
                FirefoxAccount(p)
            }
        }
    }
}
