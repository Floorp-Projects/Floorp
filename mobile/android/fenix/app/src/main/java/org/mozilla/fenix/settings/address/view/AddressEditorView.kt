/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.address.view

import android.content.Context
import android.content.DialogInterface
import androidx.appcompat.app.AlertDialog
import androidx.core.view.isVisible
import mozilla.components.concept.storage.Address
import android.widget.ArrayAdapter
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.search.RegionState
import mozilla.components.concept.storage.UpdatableAddressFields
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.ktx.android.view.showKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.FragmentAddressEditorBinding
import org.mozilla.fenix.ext.placeCursorAtEnd
import org.mozilla.fenix.settings.address.AddressEditorFragment
import org.mozilla.fenix.settings.address.interactor.AddressEditorInteractor

internal const val DEFAULT_COUNTRY = "US"

/**
 * Shows an address editor for adding or updating an address.
 */
class AddressEditorView(
    private val binding: FragmentAddressEditorBinding,
    private val interactor: AddressEditorInteractor,
    private val region: RegionState? = RegionState.Default,
    private val address: Address? = null
) {

    /**
     * Value type representing properties determined by the country used in an [Address].
     * This data is meant to mirror the data currently represented on desktop here:
     * https://searchfox.org/mozilla-central/source/toolkit/components/formautofill/addressmetadata/addressReferences.js
     *
     * This can be expanded to included things like a list of applicable states/provinces per country
     * or the names that should be used for each form field.
     *
     * Note: Most properties here need to be kept in sync with the data in the above desktop
     * address reference file in order to prevent duplications when sync is enabled. There are
     * ongoing conversations about how best to share that data cross-platform, if at all.
     * Some more detail: https://bugzilla.mozilla.org/show_bug.cgi?id=1769809
     *
     * Exceptions: [displayName] is a local property and stop-gap to a more robust solution.
     *
     * @property key The country code used to lookup the address data. Should match desktop entries.
     * @property displayName The name to display when selected.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal data class Country(
        val key: String,
        val displayName: String,
    )

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val countries = mapOf(
        "CA" to Country("CA", "Canada"),
        "US" to Country("US", "United States"),
    )

    /**
     * Binds the view in the [AddressEditorFragment], using the current [Address] if available.
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

            binding.deleteButton.apply {
                isVisible = true
                setOnClickListener { view ->
                    showConfirmDeleteAddressDialog(view.context, address.guid)
                }
            }
        }

        bindCountryDropdown()
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
            country = binding.countryDropDown.selectedItem.toString().toCountryCode(),
            tel = binding.phoneInput.text.toString(),
            email = binding.emailInput.text.toString()
        )

        if (address != null) {
            interactor.onUpdateAddress(address.guid, addressFields)
        } else {
            interactor.onSaveAddress(addressFields)
        }
    }

    internal fun showConfirmDeleteAddressDialog(context: Context, guid: String) {
        AlertDialog.Builder(context).apply {
            setMessage(R.string.addressess_confirm_dialog_message)
            setNegativeButton(R.string.addressess_confirm_dialog_cancel_button) { dialog: DialogInterface, _ ->
                dialog.cancel()
            }
            setPositiveButton(R.string.addressess_confirm_dialog_ok_button) { _, _ ->
                interactor.onDeleteAddress(guid)
            }
            create()
        }.show()
    }

    private fun bindCountryDropdown() {
        val adapter = ArrayAdapter<String>(
            binding.root.context,
            android.R.layout.simple_spinner_dropdown_item,
        )

        val selectedCountryKey = (address?.country ?: region?.home).takeIf {
            it in countries.keys
        } ?: DEFAULT_COUNTRY
        var selectedPosition = -1
        countries.values.forEachIndexed { index, country ->
            if (country.key == selectedCountryKey) selectedPosition = index
            adapter.add(country.displayName)
        }

        binding.countryDropDown.adapter = adapter
        binding.countryDropDown.setSelection(selectedPosition)
    }

    private fun String.toCountryCode() = countries.values.find {
        it.displayName == this
    }?.key ?: DEFAULT_COUNTRY
}
