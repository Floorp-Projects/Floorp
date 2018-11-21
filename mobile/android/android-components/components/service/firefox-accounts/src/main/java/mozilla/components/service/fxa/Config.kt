/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.async
import org.mozilla.fxaclient.internal.Config as InternalConfig

/**
 * Config represents the server endpoint configurations needed for the
 * authentication flow.
 */
class Config internal constructor(internal val inner: InternalConfig) : AutoCloseable {

    override fun close() {
        inner.close()
    }

    companion object {
        /**
         * Set up endpoints used in the production Firefox Accounts instance.
         *
         * @param client_id Client Id of the FxA relier
         * @param redirect_uri Redirect Uri of the FxA relier
         */
        fun release(client_id: String, redirect_uri: String): Deferred<Config> {
            return GlobalScope.async(Dispatchers.IO) {
                Config(InternalConfig.release(client_id, redirect_uri))
            }
        }

        /**
         * Set up endpoints used by a custom host for authentication
         *
         * @param content_base Hostname of the FxA auth service provider
         * @param client_id Client Id of the FxA relier
         * @param redirect_uri Redirect Uri of the FxA relier
         */
        fun custom(content_base: String, client_id: String, redirect_uri: String): Deferred<Config> {
            return GlobalScope.async(Dispatchers.IO) {
                Config(InternalConfig.custom(content_base, client_id, redirect_uri))
            }
        }
    }
}
