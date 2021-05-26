/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.storage.CreditCard
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.settings.creditcards.controller.CreditCardsManagementController
import org.mozilla.fenix.settings.creditcards.interactor.DefaultCreditCardsManagementInteractor

class DefaultCreditCardsManagementInteractorTest {

    private val controller: CreditCardsManagementController = mockk(relaxed = true)

    private lateinit var interactor: DefaultCreditCardsManagementInteractor

    @Before
    fun setup() {
        interactor = DefaultCreditCardsManagementInteractor(controller)
    }

    @Test
    fun onSelectCreditCard() {
        val creditCard: CreditCard = mockk(relaxed = true)
        interactor.onSelectCreditCard(creditCard)
        verify { controller.handleCreditCardClicked(creditCard) }
    }

    @Test
    fun onClickAddCreditCard() {
        interactor.onAddCreditCardClick()
        verify { controller.handleAddCreditCardClicked() }
    }
}
