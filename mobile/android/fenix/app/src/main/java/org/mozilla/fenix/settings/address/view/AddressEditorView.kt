/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.address.view

import android.content.Context
import android.content.DialogInterface
import androidx.appcompat.app.AlertDialog
import androidx.core.view.isVisible
import mozilla.components.concept.storage.Address
import mozilla.components.concept.storage.UpdatableAddressFields
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.ktx.android.view.showKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.FragmentAddressEditorBinding
import org.mozilla.fenix.ext.placeCursorAtEnd
import org.mozilla.fenix.settings.address.interactor.AddressEditorInteractor

/**
 * Shows an address editor for adding or updating an address.
 */
class AddressEditorView(
    private val binding: FragmentAddressEditorBinding,
    private val interactor: AddressEditorInteractor,
    private val address: Address? = null
) {

    /**
     * Binds the view.
     */
    fun bind() {
        binding.firstNameInput.apply {
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

        address?.let { address ->
            binding.emailInput.setText(address.email)
            binding.phoneInput.setText(address.tel)

            binding.firstNameInput.setText(address.givenName)
            binding.middleNameInput.setText(address.additionalName)
            binding.lastNameInput.setText(address.familyName)

            binding.streetAddressInput.setText(address.streetAddress)
            binding.cityInput.setText(address.addressLevel2)
            binding.stateInput.setText(address.addressLevel1)
            binding.zipInput.setText(address.postalCode)
        }
    }

    internal fun saveAddress() {
        binding.root.hideKeyboard()

        val addressFields = UpdatableAddressFields(
            givenName = binding.firstNameInput.text.toString(),
            additionalName = binding.middleNameInput.text.toString(),
            familyName = binding.lastNameInput.text.toString(),
            organization = "",
            streetAddress = binding.streetAddressInput.text.toString(),
            addressLevel3 = "",
            addressLevel2 = "",
            addressLevel1 = "",
            postalCode = binding.zipInput.text.toString(),
            country = "",
            tel = binding.phoneInput.text.toString(),
            email = binding.emailInput.text.toString()
        )

        if (address != null) {
            interactor.onUpdateAddress(address.guid, addressFields)
        } else {
            interactor.onSaveAddress(addressFields)
        }
    }
}
