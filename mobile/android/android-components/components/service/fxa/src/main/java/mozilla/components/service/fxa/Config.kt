/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

class Config(override var rawPointer: FxaClient.RawConfig?) : RustObject<FxaClient.RawConfig>() {

    override fun destroyPointer(p: FxaClient.RawConfig) {
        FxaClient.INSTANCE.fxa_config_free(p)
    }

    companion object {
        fun release(): FxaResult<Config> {
            val e = Error.ByReference()
            val cfg = FxaClient.INSTANCE.fxa_get_release_config(e)
            if (e.isFailure()) {
                return FxaResult.fromException(FxaException.fromConsuming(e))
            } else {
                return FxaResult.fromValue(Config(cfg))
            }
        }

        fun custom(content_base: String): FxaResult<Config> {
            val e = Error.ByReference()
            val cfg = FxaClient.INSTANCE.fxa_get_custom_config(content_base, e)
            if (e.isFailure()) {
                return FxaResult.fromException(FxaException.fromConsuming(e))
            } else {
                return FxaResult.fromValue(Config(cfg))
            }
        }
    }
}
