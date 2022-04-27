/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import mozilla.components.concept.fetch.Client

/**
 * Possible actions for Pocket usecases which need to access the Internet.
 */
internal interface PocketNetworkUseCases {
    /**
     * HTTP client used for network requests.
     * Can be set through [initialize] or unset through [reset].
     */
    var fetchClient: Client?

    /**
     * Convenience method for setting the the HTTP Client which will be used
     * for all REST communications with the Pocket server.
     */
    fun initialize(client: Client)

    /**
     * Convenience method for cleaning up any resources held for communicating with the Pocket server.
     */
    fun reset()
}

/**
 * Base class for Pocket usecases which need to access the Internet.
 * Allow to easily set and get a [Client] reference through static methods.
 */
@Suppress("UtilityClassWithPublicConstructor")
internal open class PocketNetworkUseCase {
    companion object : PocketNetworkUseCases {
        override var fetchClient: Client? = null

        /**
         * Convenience method for setting the the HTTP Client which will be used
         * for all REST communications with the Pocket server.
         *
         * Already downloaded data can still be queried but no new data can be downloaded until
         * this parameter is set.
         */
        override fun initialize(client: Client) {
            fetchClient = client
        }

        /**
         * Convenience method for cleaning up any resources held for communicating with the Pocket server.
         *
         * Already downloaded data can still be queried but no new data can be downloaded until
         * [initialize] is used again.
         */
        override fun reset() {
            fetchClient = null
        }
    }
}
