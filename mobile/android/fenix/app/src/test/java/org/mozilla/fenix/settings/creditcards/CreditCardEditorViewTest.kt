/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import android.view.LayoutInflater
import android.view.View
import io.mockk.mockk
import io.mockk.verify
import kotlinx.android.synthetic.main.fragment_credit_card_editor.view.*
import mozilla.components.concept.storage.CreditCard
import mozilla.components.concept.storage.UpdatableCreditCardFields
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.creditcards.CreditCardEditorFragment.Companion.NUMBER_OF_YEARS_TO_SHOW
import org.mozilla.fenix.settings.creditcards.interactor.CreditCardEditorInteractor
import org.mozilla.fenix.settings.creditcards.view.CreditCardEditorView
import java.util.Calendar

@RunWith(FenixRobolectricTestRunner::class)
class CreditCardEditorViewTest {

    private lateinit var view: View
    private lateinit var interactor: CreditCardEditorInteractor
    private lateinit var creditCardEditorView: CreditCardEditorView

    private val creditCard = CreditCard(
        guid = "id",
        billingName = "Banana Apple",
        cardNumber = "4111111111111110",
        expiryMonth = 5,
        expiryYear = 2030,
        cardType = "amex",
        timeCreated = 1L,
        timeLastUsed = 1L,
        timeLastModified = 1L,
        timesUsed = 1L
    )

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext).inflate(R.layout.fragment_credit_card_editor, null)
        interactor = mockk(relaxed = true)

        creditCardEditorView = CreditCardEditorView(view, interactor)
    }

    @Test
    fun `GIVEN the initial credit card editor state THEN credit card form inputs are in initial state`() {
        creditCardEditorView.bind(getInitialCreditCardEditorState())

        val calendar = Calendar.getInstance()
        val startYear = calendar.get(Calendar.YEAR)
        val endYear = startYear + NUMBER_OF_YEARS_TO_SHOW - 1

        assertEquals("", view.card_number_input.text.toString())
        assertEquals("", view.name_on_card_input.text.toString())

        with(view.expiry_month_drop_down) {
            assertEquals(12, count)
            assertEquals("January (01)", selectedItem.toString())
            assertEquals("December (12)", getItemAtPosition(count - 1).toString())
        }

        with(view.expiry_year_drop_down) {
            assertEquals(10, count)
            assertEquals(startYear.toString(), selectedItem.toString())
            assertEquals(endYear.toString(), getItemAtPosition(count - 1).toString())
        }
    }

    @Test
    fun `GIVEN a credit card THEN credit card form inputs are displaying the provided credit card information`() {
        creditCardEditorView.bind(creditCard.toCreditCardEditorState())

        assertEquals(creditCard.cardNumber, view.card_number_input.text.toString())
        assertEquals(creditCard.billingName, view.name_on_card_input.text.toString())

        with(view.expiry_month_drop_down) {
            assertEquals(12, count)
            assertEquals("May (05)", selectedItem.toString())
        }

        with(view.expiry_year_drop_down) {
            val endYear = creditCard.expiryYear + NUMBER_OF_YEARS_TO_SHOW - 1

            assertEquals(10, count)
            assertEquals(creditCard.expiryYear.toString(), selectedItem.toString())
            assertEquals(endYear.toString(), getItemAtPosition(count - 1).toString())
        }
    }

    @Test
    fun `WHEN the cancel button is clicked THEN interactor is called`() {
        creditCardEditorView.bind(getInitialCreditCardEditorState())

        view.cancel_button.performClick()

        verify { interactor.onCancelButtonClicked() }
    }

    @Test
    fun `GIVEN a credit card WHEN the save button is clicked THEN interactor is called`() {
        creditCardEditorView.bind(creditCard.toCreditCardEditorState())

        view.save_button.performClick()

        verify {
            interactor.onSaveButtonClicked(
                UpdatableCreditCardFields(
                    billingName = creditCard.billingName,
                    cardNumber = creditCard.cardNumber,
                    expiryMonth = creditCard.expiryMonth,
                    expiryYear = creditCard.expiryYear,
                    cardType = "amex"
                )
            )
        }
    }
}
