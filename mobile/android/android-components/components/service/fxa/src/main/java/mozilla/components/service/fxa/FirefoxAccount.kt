/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import kotlinx.coroutines.experimental.launch
import kotlinx.coroutines.experimental.runBlocking

class FirefoxAccount(override var rawPointer: RawFxAccount?) : RustObject<RawFxAccount>() {

    constructor(config: Config, clientId: String): this(null) {
        val e = Error.ByReference()
        val result = FxaClient.INSTANCE.fxa_new(config.consumePointer(), clientId, e)
        if (e.isFailure()) throw FxaException.fromConsuming(e)
        this.rawPointer = result
    }

    override fun destroy(p: RawFxAccount) {
        runBlocking(FxaClient.THREAD_CONTEXT) { FxaClient.INSTANCE.fxa_free(p) }
    }

    fun beginOAuthFlow(redirectURI: String, scopes: Array<String>, wantsKeys: Boolean): FxaResult<String> {
        val result = FxaResult<String>()
        launch(FxaClient.THREAD_CONTEXT) {
            val scope = scopes.joinToString(" ")
            val e = Error.ByReference()
            val p = FxaClient.INSTANCE.fxa_begin_oauth_flow(validPointer(), redirectURI, scope, wantsKeys, e)
            if (e.isFailure()) {
                result.completeExceptionally(FxaException.fromConsuming(e))
            } else {
                result.complete(getAndConsumeString(p))
            }
        }
        return result
    }

    fun getProfile(ignoreCache: Boolean): FxaResult<Profile> {
        val result = FxaResult<Profile>()
        launch(FxaClient.THREAD_CONTEXT) {
            val e = Error.ByReference()
            val p = FxaClient.INSTANCE.fxa_profile(validPointer(), ignoreCache, e)
            if (e.isFailure()) {
                result.completeExceptionally(FxaException.fromConsuming(e))
            } else {
                result.complete(Profile(p))
            }
        }
        return result
    }

    fun getProfile(): FxaResult<Profile> {
        return getProfile(false)
    }

    fun newAssertion(audience: String): String? {
        return runBlocking(FxaClient.THREAD_CONTEXT) {
            val e = Error.ByReference()
            val p = FxaClient.INSTANCE.fxa_assertion_new(validPointer(), audience, e)
            if (e.isFailure()) throw FxaException.fromConsuming(e)
            getAndConsumeString(p)
        }
    }

    fun getTokenServerEndpointURL(): String? {
        return runBlocking(FxaClient.THREAD_CONTEXT) {
            val e = Error.ByReference()
            val p = FxaClient.INSTANCE.fxa_get_token_server_endpoint_url(validPointer(), e)
            if (e.isFailure()) throw FxaException.fromConsuming(e)
            getAndConsumeString(p)
        }
    }

    fun getSyncKeys(): SyncKeys {
        return runBlocking(FxaClient.THREAD_CONTEXT) {
            val e = Error.ByReference()
            val p = FxaClient.INSTANCE.fxa_get_sync_keys(validPointer(), e)
            if (e.isFailure()) throw FxaException.fromConsuming(e)
            SyncKeys(p)
        }
    }

    fun completeOAuthFlow(code: String, state: String): FxaResult<OAuthInfo> {
        val result = FxaResult<OAuthInfo>()
        launch(FxaClient.THREAD_CONTEXT) {
            val e = Error.ByReference()
            val p = FxaClient.INSTANCE.fxa_complete_oauth_flow(validPointer(), code, state, e)
            if (e.isFailure()) {
                result.completeExceptionally(FxaException.fromConsuming(e))
            } else {
                result.complete(OAuthInfo(p))
            }
        }
        return result
    }

    fun getOAuthToken(scopes: Array<String>): FxaResult<OAuthInfo> {
        val result = FxaResult<OAuthInfo>()
        launch(FxaClient.THREAD_CONTEXT) {
            val scope = scopes.joinToString(" ")
            val e = Error.ByReference()
            val p = FxaClient.INSTANCE.fxa_get_oauth_token(validPointer(), scope, e)
            if (e.isFailure()) {
                result.completeExceptionally(FxaException.fromConsuming(e))
            } else {
                result.complete(OAuthInfo(p))
            }
        }
        return result
    }

    companion object {
        fun from(config: Config, clientId: String, webChannelResponse: String): FxaResult<FirefoxAccount> {
            val result = FxaResult<FirefoxAccount>()
            launch(FxaClient.THREAD_CONTEXT) {
                val e = Error.ByReference()
                val raw = FxaClient.INSTANCE.fxa_from_credentials(config.consumePointer(),
                        clientId, webChannelResponse, e)
                if (e.isFailure()) {
                    result.completeExceptionally(FxaException.fromConsuming(e))
                } else {
                    result.complete(FirefoxAccount(raw))
                }
            }
            return result
        }

        fun fromJSONString(json: String): FxaResult<FirefoxAccount> {
            val result = FxaResult<FirefoxAccount>()
            launch(FxaClient.THREAD_CONTEXT) {
                val e = Error.ByReference()
                val raw = FxaClient.INSTANCE.fxa_from_json(json, e)
                if (e.isFailure()) {
                    result.completeExceptionally(FxaException.fromConsuming(e))
                } else {
                    result.complete(FirefoxAccount(raw))
                }
            }
            return result
        }
    }
}
