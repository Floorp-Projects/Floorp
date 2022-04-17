/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.address.view

import mozilla.components.concept.storage.UpdatableAddressFields
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.ktx.android.view.showKeyboard
import org.mozilla.fenix.databinding.FragmentAddressEditorBinding
import org.mozilla.fenix.ext.placeCursorAtEnd
import org.mozilla.fenix.settings.address.interactor.AddressEditorInteractor

/**
 * Shows an address editor for adding or updating an address.
 */
class AddressEditorView(
    private val binding: FragmentAddressEditorBinding,
    private val interactor: AddressEditorInteractor
) {

    /**
     * Binds the view.
     */
    fun bind() {
        binding.fullNameInput.apply {
            requestFocus()
            placeCursorAtEnd()
            showKeyboard()
        }

        binding.cancelButton.setOnClickListener {
            interactor.onCancelButtonClicked()
        }

        binding.saveButton.setOnClickListener {
            saveAddress()
        }
    }

    internal fun saveAddress() {
        binding.root.hideKeyboard()

        interactor.onSaveAddress(
            UpdatableAddressFields(
                givenName = binding.fullNameInput.text.toString(),
                additionalName = "",
                familyName = "",
                organization = "",
                streetAddress = binding.streetAddressInput.text.toString(),
                addressLevel3 = "",
                addressLevel2 = "",
                addressLevel1 = "",
                postalCode = binding.zipInput.text.toString(),
                country = "",
                tel = binding.phoneInput.toString(),
                email = binding.emailInput.toString()
            )
        )
    }
}
