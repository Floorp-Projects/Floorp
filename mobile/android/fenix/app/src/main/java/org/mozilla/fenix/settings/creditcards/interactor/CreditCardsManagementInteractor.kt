/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards.interactor

import org.mozilla.fenix.settings.creditcards.controller.CreditCardsManagementController

/**
 * Interface for the credit cards management Interactor.
 */
interface CreditCardsManagementInteractor {

    /**
     * Navigates to the credit card editor to edit the selected credit card. Called when a user
     * taps on a credit card item.
     */
    fun onSelectCreditCard()
}

/**
 * The default implementation of [CreditCardEditorInteractor]
 */
class DefaultCreditCardsManagementInteractor(
    private val controller: CreditCardsManagementController
) : CreditCardsManagementInteractor {

    override fun onSelectCreditCard() {
        controller.handleCreditCardClicked()
    }
}
