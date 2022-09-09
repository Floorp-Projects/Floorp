/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards.interactor

import mozilla.components.concept.storage.NewCreditCardFields
import mozilla.components.concept.storage.UpdatableCreditCardFields
import org.mozilla.fenix.settings.creditcards.controller.CreditCardEditorController

/**
 * Interface for the credit card editor Interactor.
 */
interface CreditCardEditorInteractor {

    /**
     * Navigates back to the credit card preference settings. Called when a user taps on the
     * "Cancel" button.
     */
    fun onCancelButtonClicked()

    /**
     * Deletes the provided credit card in the credit card storage. Called when a user
     * taps on the delete menu item or "Delete card" button.
     *
     * @param guid Unique identifier for the credit card to be deleted.
     */
    fun onDeleteCardButtonClicked(guid: String)

    /**
     * Saves the provided credit card field into the credit card storage. Called when a user
     * taps on the save menu item or "Save" button.
     *
     * @param creditCardFields A [NewCreditCardFields] record to add.
     */
    fun onSaveCreditCard(creditCardFields: NewCreditCardFields)

    /**
     * Updates the provided credit card with the new credit card fields. Called when a user
     * taps on the save menu item or "Save" button when editing an existing credit card.
     *
     * @param guid Unique identifier for the desired credit card.
     * @param creditCardFields The credit card fields to update.
     */
    fun onUpdateCreditCard(guid: String, creditCardFields: UpdatableCreditCardFields)
}

/**
 * The default implementation of [CreditCardEditorInteractor].
 *
 * @param controller An instance of [CreditCardEditorController] which will be delegated for all
 * user interactions.
 */
class DefaultCreditCardEditorInteractor(
    private val controller: CreditCardEditorController,
) : CreditCardEditorInteractor {

    override fun onCancelButtonClicked() {
        controller.handleCancelButtonClicked()
    }

    override fun onDeleteCardButtonClicked(guid: String) {
        controller.handleDeleteCreditCard(guid)
    }

    override fun onSaveCreditCard(creditCardFields: NewCreditCardFields) {
        controller.handleSaveCreditCard(creditCardFields)
    }

    override fun onUpdateCreditCard(guid: String, creditCardFields: UpdatableCreditCardFields) {
        controller.handleUpdateCreditCard(guid, creditCardFields)
    }
}
