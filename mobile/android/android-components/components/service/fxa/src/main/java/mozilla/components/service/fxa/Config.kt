/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import kotlinx.coroutines.experimental.launch

class Config(override var rawPointer: RawConfig?) : RustObject<RawConfig>() {
    override fun destroy(p: RawConfig) {
        FxaClient.INSTANCE.fxa_config_free(p)
    }

    companion object {
        fun release(): FxaResult<Config> {
            val result = FxaResult<Config>()
            val e = Error.ByReference()
            launch(FxaClient.THREAD_CONTEXT) {
                val cfg = FxaClient.INSTANCE.fxa_get_release_config(e)
                if (e.isFailure()) {
                    result.completeExceptionally(FxaException.fromConsuming(e))
                } else {
                    result.complete(Config(cfg))
                }
            }
            return result
        }

        fun custom(content_base: String): FxaResult<Config> {
            val result = FxaResult<Config>()
            val e = Error.ByReference()
            launch(FxaClient.THREAD_CONTEXT) {
                val cfg = FxaClient.INSTANCE.fxa_get_custom_config(content_base, e)
                if (e.isFailure()) {
                    result.completeExceptionally(FxaException.fromConsuming(e))
                } else {
                    result.complete(Config(cfg))
                }
            }
            return result
        }
    }
}
