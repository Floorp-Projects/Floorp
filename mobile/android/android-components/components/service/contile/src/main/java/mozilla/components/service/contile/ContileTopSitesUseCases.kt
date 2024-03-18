/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.contile

import androidx.annotation.VisibleForTesting

/**
 * Contains use cases related to the Contlie top sites feature.
 */
internal class ContileTopSitesUseCases {

    /**
     * Refresh Contile top sites use case.
     */
    class RefreshContileTopSitesUseCase internal constructor() {
        /**
         * Refreshes the Contile top sites.
         */
        suspend operator fun invoke() {
            requireContileTopSitesProvider().getTopSites(allowCache = false)
        }
    }

    internal companion object {
        @VisibleForTesting internal var provider: ContileTopSitesProvider? = null

        /**
         * Initializes the [ContileTopSitesProvider] which will providde access to the Contile
         * services API.
         */
        internal fun initialize(provider: ContileTopSitesProvider) {
            this.provider = provider
        }

        /**
         * Unbinds the [ContileTopSitesProvider].
         */
        internal fun destroy() {
            this.provider = null
        }

        /**
         * Returns the [ContileTopSitesProvider], otherwise throw an exception if the [provider]
         * has not been initialized.
         */
        internal fun requireContileTopSitesProvider(): ContileTopSitesProvider {
            return requireNotNull(provider) {
                "initialize must be called before trying to access the ContileTopSitesProvider"
            }
        }
    }

    val refreshContileTopSites: RefreshContileTopSitesUseCase by lazy {
        RefreshContileTopSitesUseCase()
    }
}
