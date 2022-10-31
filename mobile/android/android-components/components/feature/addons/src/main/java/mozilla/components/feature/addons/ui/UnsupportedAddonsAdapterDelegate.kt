/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.ui

/**
 * Provides methods for handling the success and error callbacks from uninstalling an add-on in the
 * list of unsupported add-on items.
 */
interface UnsupportedAddonsAdapterDelegate {
    /**
     * Callback invoked if the addon was uninstalled successfully.
     */
    fun onUninstallSuccess() = Unit

    /**
     * Callback invoked if there was an error uninstalling the addon.
     *
     * @param addonId The unique id of the [Addon].
     * @param throwable An exception to log.
     */
    fun onUninstallError(addonId: String, throwable: Throwable) = Unit
}
