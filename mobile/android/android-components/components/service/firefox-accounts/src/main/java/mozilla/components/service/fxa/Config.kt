/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

/**
 * Config represents the server endpoint configurations needed for the
 * authentication flow.
 */
class Config(override var rawPointer: RawConfig?) : RustObject<RawConfig>() {
    override fun destroy(p: RawConfig) {
        synchronized(FxaClient.INSTANCE) { FxaClient.INSTANCE.fxa_config_free(p) }
    }

    companion object {
        /**
         * Set up endpoints used in the production Firefox Accounts instance.
         */
        fun release(): FxaResult<Config> {
            return safeAsync { e ->
                val p = FxaClient.INSTANCE.fxa_get_release_config(e)
                Config(p)
            }
        }

        /**
         * Set up endpoints used by a custom host for authentication
         *
         * @param content_base Hostname of the FxA auth service provider
         */
        fun custom(content_base: String): FxaResult<Config> {
            return safeAsync { e ->
                val p = FxaClient.INSTANCE.fxa_get_custom_config(content_base, e)
                Config(p)
            }
        }
    }
}
