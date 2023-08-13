/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.address.interactor

import mozilla.components.concept.storage.Address
import org.mozilla.fenix.settings.address.controller.AddressManagementController

/**
 * Interface for the address management interactor.
 */
interface AddressManagementInteractor {

    /**
     * Navigates to the address editor to edit the selected address. Called when a user
     * taps on an address item.
     *
     * @param address The selected [Address] to edit.
     */
    fun onSelectAddress(address: Address)

    /**
     * Navigates to the address editor to add a new address. Called when a user
     * taps on 'Add address' button.
     */
    fun onAddAddressButtonClick()
}

/**
 * The default implementation of [AddressManagementInteractor].
 *
 * @property controller An instance of [AddressManagementController] which will be delegated for
 * all user interactions.
 */
class DefaultAddressManagementInteractor(
    private val controller: AddressManagementController,
) : AddressManagementInteractor {

    override fun onSelectAddress(address: Address) {
        controller.handleAddressClicked(address)
    }

    override fun onAddAddressButtonClick() {
        controller.handleAddAddressButtonClicked()
    }
}
