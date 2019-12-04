/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons

import mozilla.components.browser.state.action.WebExtensionAction
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.webextensions.WebExtensionSupport

/**
 * Provides access to installed and recommended [Addon]s and manages their states.
 *
 * @property store The application's [BrowserStore].
 * @property addonsProvider The [AddonsProvider] to query available [Addon]s.
 */
class AddonManager(
    private val store: BrowserStore,
    private val addonsProvider: AddonsProvider
) {

    /**
     * Returns the list of all installed and recommended add-ons.
     *
     * @return list of all [Addon]s with up-to-date [Addon.installedState].
     * @throws AddonManagerException in case of a problem reading from
     * the [addonsProvider] or querying web extension state from the engine / store.
     */
    @Throws(AddonManagerException::class)
    @Suppress("TooGenericExceptionCaught")
    suspend fun getAddons(): List<Addon> {
        try {
            // Make sure extension support is initialized, i.e. the state of all installed extensions is known.
            WebExtensionSupport.awaitInitialization()

            // TODO for testing purposes -> remove once management API lands
            val ublockId = "607454"
            store.dispatch(WebExtensionAction.InstallWebExtensionAction(WebExtensionState(ublockId))).join()

            return addonsProvider.getAvailableAddons().map { addon ->
                store.state.extensions[addon.id]?.let { installedAddon ->
                    // TODO Add fields once we get the management API:
                    // https://github.com/mozilla-mobile/android-components/issues/4500
                    addon.copy(installedState = Addon.InstalledState(installedAddon.id, "1.0", "https://mozilla.org"))
                } ?: addon
            }
        } catch (throwable: Throwable) {
            throw AddonManagerException(throwable)
        }
    }
}

/**
 * Wraps exceptions thrown by either the initialization process or an [AddonsProvider].
 */
class AddonManagerException(throwable: Throwable) : Exception(throwable)
