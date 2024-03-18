/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.address.interactor

import mozilla.components.concept.storage.Address
import mozilla.components.concept.storage.UpdatableAddressFields
import org.mozilla.fenix.settings.address.controller.AddressEditorController

/**
 * Interface for the address editor interactor.
 */
interface AddressEditorInteractor {

    /**
     * Navigates back to the autofill preference settings. Called when a user taps on the
     * "Cancel" button.
     */
    fun onCancelButtonClicked()

    /**
     * Saves the provided address field into the autofill storage. Called when a user
     * taps on the save menu item or "Save" button.
     *
     * @param addressFields A [UpdatableAddressFields] record to add.
     */
    fun onSaveAddress(addressFields: UpdatableAddressFields)

    /**
     * Deletes the provided address from the autofill storage. Called when a user
     * taps on the save menu item or "Save" button.
     *
     * @param guid The unique identifier for the [Address] record to delete.
     */
    fun onDeleteAddress(guid: String)

    /**
     * Updates the provided address in the autofill storage. Called when a user
     * taps on the update menu item or "Update" button.
     *
     * @param guid The unique identifier for the [Address] record to update.
     * @param addressFields A [UpdatableAddressFields] record to add.
     */
    fun onUpdateAddress(guid: String, addressFields: UpdatableAddressFields)
}

/**
 * The default implementation of [AddressEditorInteractor].
 *
 * @param controller An instance of [AddressEditorController] which will be delegated for all
 * user interactions.
 */
class DefaultAddressEditorInteractor(
    private val controller: AddressEditorController,
) : AddressEditorInteractor {

    override fun onCancelButtonClicked() {
        controller.handleCancelButtonClicked()
    }

    override fun onSaveAddress(addressFields: UpdatableAddressFields) {
        controller.handleSaveAddress(addressFields)
    }

    override fun onDeleteAddress(guid: String) {
        controller.handleDeleteAddress(guid)
    }

    override fun onUpdateAddress(guid: String, addressFields: UpdatableAddressFields) {
        controller.handleUpdateAddress(guid, addressFields)
    }
}
