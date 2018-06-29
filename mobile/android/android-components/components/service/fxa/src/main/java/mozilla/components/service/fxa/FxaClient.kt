/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import com.sun.jna.Library
import com.sun.jna.Native
import com.sun.jna.Pointer
import com.sun.jna.NativeLibrary
import com.sun.jna.PointerType

@Suppress("FunctionNaming", "TooManyFunctions")
interface FxaClient : Library {
    companion object {
        private const val JNA_LIBRARY_NAME = "fxa_client"
        lateinit var JNA_NATIVE_LIB: Any
        lateinit var INSTANCE: FxaClient

        fun init() {
            System.loadLibrary("crypto")
            System.loadLibrary("ssl")
            System.loadLibrary("fxa_client")
            JNA_NATIVE_LIB = NativeLibrary.getInstance(JNA_LIBRARY_NAME)
            INSTANCE = Native.loadLibrary(JNA_LIBRARY_NAME, FxaClient::class.java) as FxaClient
        }
    }

    // This is ultra hacky and takes advantage of the zero-based error codes returned in Rust.
    enum class ErrorCode {
        NoError, Other, AuthenticationError, InternalPanic
    }

    class RawFxAccount : PointerType()
    class RawConfig : PointerType()

    fun fxa_get_release_config(e: Error.ByReference): RawConfig
    fun fxa_get_custom_config(content_base: String, e: Error.ByReference): RawConfig

    fun fxa_new(config: RawConfig, clientId: String, e: Error.ByReference): RawFxAccount
    fun fxa_from_credentials(
        config: RawConfig,
        clientId: String,
        webChannelResponse: String,
        e: Error.ByReference
    ): RawFxAccount

    fun fxa_from_json(json: String, e: Error.ByReference): RawFxAccount

    fun fxa_begin_oauth_flow(
        fxa: RawFxAccount,
        redirectUri: String,
        scopes: String,
        wantsKeys: Boolean,
        e: Error.ByReference
    ): Pointer

    fun fxa_profile(fxa: RawFxAccount, ignoreCache: Boolean, e: Error.ByReference): Profile.Raw
    fun fxa_assertion_new(fxa: RawFxAccount, audience: String, e: Error.ByReference): Pointer
    fun fxa_get_token_server_endpoint_url(fxa: RawFxAccount, e: Error.ByReference): Pointer

    fun fxa_get_sync_keys(fxa: RawFxAccount, e: Error.ByReference): SyncKeys.Raw

    fun fxa_complete_oauth_flow(fxa: RawFxAccount, code: String, state: String, e: Error.ByReference): OAuthInfo.Raw
    fun fxa_get_oauth_token(fxa: RawFxAccount, scope: String, e: Error.ByReference): OAuthInfo.Raw

    fun fxa_config_free(config: RawConfig)
    fun fxa_str_free(string: Pointer)
    fun fxa_free(fxa: RawFxAccount)

    // In theory these would take `OAuthInfo.Raw.ByReference` (and etc), but
    // the rust functions that return these return `OAuthInfo.Raw` and not
    // the ByReference subtypes. So I'm not sure there's a way to do this
    // when using Structure.
    fun fxa_oauth_info_free(ptr: Pointer)

    fun fxa_profile_free(ptr: Pointer)
    fun fxa_sync_keys_free(ptr: Pointer)
}
