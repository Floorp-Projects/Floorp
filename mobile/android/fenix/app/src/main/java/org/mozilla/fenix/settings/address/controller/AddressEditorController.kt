/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.address.controller

import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.concept.storage.UpdatableAddressFields
import mozilla.components.service.sync.autofill.AutofillCreditCardsAddressesStorage
import org.mozilla.fenix.settings.address.AddressEditorFragment
import org.mozilla.fenix.settings.address.interactor.AddressEditorInteractor

/**
 * [AddressEditorFragment] controller. An interface that handles the view manipulation of the
 * credit card editor.
 */
interface AddressEditorController {

    /**
     * @see [AddressEditorInteractor.onCancelButtonClicked]
     */
    fun handleCancelButtonClicked()

    /**
     * @see [AddressEditorInteractor.onSaveAddress]
     */
    fun handleSaveAddress(addressFields: UpdatableAddressFields)

    /**
     * @see [AddressEditorInteractor.onDeleteAddress]
     */
    fun handleDeleteAddress(guid: String)

    /**
     * @see [AddressEditorInteractor.onUpdateAddress]
     */
    fun handleUpdateAddress(guid: String, addressFields: UpdatableAddressFields)
}

/**
 * The default implementation of [AddressEditorController].
 *
 * @param storage An instance of the [AutofillCreditCardsAddressesStorage] for adding and retrieving
 * addresses.
 * @param lifecycleScope [CoroutineScope] scope to launch coroutines.
 * @param navController [NavController] used for navigation.
 */
class DefaultAddressEditorController(
    private val storage: AutofillCreditCardsAddressesStorage,
    private val lifecycleScope: CoroutineScope,
    private val navController: NavController,
) : AddressEditorController {

    override fun handleCancelButtonClicked() {
        navController.popBackStack()
    }

    override fun handleSaveAddress(addressFields: UpdatableAddressFields) {
        lifecycleScope.launch {
            storage.addAddress(addressFields)

            lifecycleScope.launch(Dispatchers.Main) {
                navController.popBackStack()
            }
        }
    }

    override fun handleDeleteAddress(guid: String) {
        lifecycleScope.launch {
            storage.deleteAddress(guid)

            lifecycleScope.launch(Dispatchers.Main) {
                navController.popBackStack()
            }
        }
    }

    override fun handleUpdateAddress(guid: String, addressFields: UpdatableAddressFields) {
        lifecycleScope.launch {
            storage.updateAddress(guid, addressFields)

            lifecycleScope.launch(Dispatchers.Main) {
                navController.popBackStack()
            }
        }
    }
}
