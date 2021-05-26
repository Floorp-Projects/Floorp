/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import android.view.LayoutInflater
import android.view.View
import io.mockk.mockk
import io.mockk.verify
import kotlinx.android.synthetic.main.credit_card_list_item.view.*
import mozilla.components.concept.storage.CreditCard
import mozilla.components.concept.storage.CreditCardNumber
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.utils.CreditCardNetworkType
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.creditcards.interactor.CreditCardsManagementInteractor
import org.mozilla.fenix.settings.creditcards.view.CreditCardItemViewHolder

@RunWith(FenixRobolectricTestRunner::class)
class CreditCardItemViewHolderTest {

    private lateinit var view: View
    private lateinit var interactor: CreditCardsManagementInteractor

    private val creditCard = CreditCard(
        guid = "id",
        billingName = "Banana Apple",
        encryptedCardNumber = CreditCardNumber.Encrypted("4111111111111110"),
        cardNumberLast4 = "1110",
        expiryMonth = 1,
        expiryYear = 2030,
        cardType = CreditCardNetworkType.AMEX.cardName,
        timeCreated = 1L,
        timeLastUsed = 1L,
        timeLastModified = 1L,
        timesUsed = 1L
    )

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext).inflate(CreditCardItemViewHolder.LAYOUT_ID, null)
        interactor = mockk(relaxed = true)
    }

    @Test
    fun `GIVEN a new credit card item on bind THEN set the card number and expiry date text`() {
        CreditCardItemViewHolder(view, interactor).bind(creditCard)

        assertEquals(creditCard.obfuscatedCardNumber, view.credit_card_number.text)
        assertEquals("0${creditCard.expiryMonth}/${creditCard.expiryYear}", view.expiry_date.text)
    }

    @Test
    fun `WHEN a credit item is clicked THEN interactor is called`() {
        CreditCardItemViewHolder(view, interactor).bind(creditCard)

        view.performClick()
        verify { interactor.onSelectCreditCard(creditCard) }
    }
}
