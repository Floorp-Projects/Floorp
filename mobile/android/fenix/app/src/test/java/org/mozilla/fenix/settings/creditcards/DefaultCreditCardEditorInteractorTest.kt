/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.storage.UpdatableCreditCardFields
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.settings.creditcards.controller.CreditCardEditorController
import org.mozilla.fenix.settings.creditcards.interactor.DefaultCreditCardEditorInteractor

class DefaultCreditCardEditorInteractorTest {

    private val controller: CreditCardEditorController = mockk(relaxed = true)

    private lateinit var interactor: DefaultCreditCardEditorInteractor

    @Before
    fun setup() {
        interactor = DefaultCreditCardEditorInteractor(controller)
    }

    @Test
    fun onCancelButtonClicked() {
        interactor.onCancelButtonClicked()
        verify { controller.handleCancelButtonClicked() }
    }

    @Test
    fun onSaveButtonClicked() {
        val creditCardFields = UpdatableCreditCardFields(
            billingName = "Banana Apple",
            cardNumber = "4111111111111112",
            expiryMonth = 1,
            expiryYear = 2030,
            cardType = "discover"
        )
        interactor.onSaveButtonClicked(creditCardFields)
        verify { controller.handleSaveCreditCard(creditCardFields) }
    }
}
