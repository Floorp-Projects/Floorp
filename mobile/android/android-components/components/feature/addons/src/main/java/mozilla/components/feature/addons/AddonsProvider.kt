/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons

/**
 * A contract that indicate how an add-on provider must behave.
 */
interface AddonsProvider {

    /**
     * Provides a list of all available add-ons.
     *
     * @param allowCache whether or not the result may be provided
     * from a previously cached response, defaults to true.
     * @param readTimeoutInSeconds optional timeout in seconds to use when fetching
     * available add-ons from a remote endpoint.
     * @param language indicates in which language the translatable fields should be in, if no
     * matching language is found then a fallback translation is returned using the default language.
     * When it is null all translations available will be returned.
     */
    suspend fun getAvailableAddons(
        allowCache: Boolean = true,
        readTimeoutInSeconds: Long? = null,
        language: String? = null,
    ): List<Addon>
}
