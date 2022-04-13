/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.storage.CreditCard
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.CreditCards
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.creditcards.controller.CreditCardsManagementController
import org.mozilla.fenix.settings.creditcards.interactor.DefaultCreditCardsManagementInteractor

@RunWith(FenixRobolectricTestRunner::class)
class DefaultCreditCardsManagementInteractorTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private val controller: CreditCardsManagementController = mockk(relaxed = true)

    private lateinit var interactor: DefaultCreditCardsManagementInteractor

    @Before
    fun setup() {
        interactor = DefaultCreditCardsManagementInteractor(controller)
    }

    @Test
    fun onSelectCreditCard() {
        val creditCard: CreditCard = mockk(relaxed = true)
        assertFalse(CreditCards.managementCardTapped.testHasValue())

        interactor.onSelectCreditCard(creditCard)
        verify { controller.handleCreditCardClicked(creditCard) }
        assertTrue(CreditCards.managementCardTapped.testHasValue())
    }

    @Test
    fun onClickAddCreditCard() {
        assertFalse(CreditCards.managementAddTapped.testHasValue())

        interactor.onAddCreditCardClick()
        verify { controller.handleAddCreditCardClicked() }
        assertTrue(CreditCards.managementAddTapped.testHasValue())
    }
}
