/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.ui

import mozilla.components.feature.addons.Addon

/**
 * Provides methods for handling the add-on items in the add-on manager.
 */
interface AddonsManagerAdapterDelegate {
    /**
     * Handler for when an add-on item is clicked.
     *
     * @param addon The [Addon] that was clicked.
     */
    fun onAddonItemClicked(addon: Addon) = Unit

    /**
     * Handler for when the install add-on button is clicked.
     *
     * @param addon The [Addon] to install.
     */
    fun onInstallAddonButtonClicked(addon: Addon) = Unit

    /**
     * Handler for when the not yet supported section is clicked.
     *
     * @param unsupportedAddons The list of unsupported [Addon].
     */
    fun onNotYetSupportedSectionClicked(unsupportedAddons: List<Addon>) = Unit
}
