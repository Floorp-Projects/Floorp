/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.creditcard

import android.widget.LinearLayout
import androidx.appcompat.widget.AppCompatTextView
import androidx.core.view.isVisible
import androidx.recyclerview.widget.RecyclerView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.prompt.CreditCard
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.concept.SelectablePromptView
import mozilla.components.support.test.ext.appCompatContext
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class CreditCardSelectBarTest {

    private lateinit var creditCardSelectBar: CreditCardSelectBar

    private val creditCard = CreditCard(
        guid = "1",
        name = "Banana Apple",
        number = "4111111111111110",
        expiryMonth = "5",
        expiryYear = "2030",
        cardType = ""
    )

    @Before
    fun setup() {
        creditCardSelectBar = CreditCardSelectBar(appCompatContext)
    }

    @Test
    fun `GIVEN a list of credit cards WHEN prompt is shown THEN credit cards are shown`() {
        val creditCards = listOf(creditCard)

        creditCardSelectBar.showPrompt(creditCards)

        assertTrue(creditCardSelectBar.isVisible)
    }

    @Test
    fun `WHEN the prompt is hidden THEN view is hidden`() {
        creditCardSelectBar.hidePrompt()

        assertFalse(creditCardSelectBar.isVisible)
    }

    @Test
    fun `GIVEN a listener WHEN manage credit cards button is clicked THEN onManageOptions is called`() {
        val listener: SelectablePromptView.Listener<CreditCard> = mock()

        assertNull(creditCardSelectBar.listener)

        creditCardSelectBar.listener = listener

        creditCardSelectBar.showPrompt(listOf(creditCard))
        creditCardSelectBar.findViewById<AppCompatTextView>(R.id.manage_credit_cards).performClick()

        verify(listener).onManageOptions()
    }

    @Test
    fun `GIVEN a listener WHEN a credit card is selected THEN onOptionSelect is called`() {
        val listener: SelectablePromptView.Listener<CreditCard> = mock()
        creditCardSelectBar.listener = listener

        creditCardSelectBar.showPrompt(listOf(creditCard))

        val adapter = creditCardSelectBar.findViewById<RecyclerView>(R.id.credit_cards_list).adapter as CreditCardsAdapter
        val holder = adapter.onCreateViewHolder(LinearLayout(testContext), 0)
        adapter.bindViewHolder(holder, 0)

        holder.itemView.performClick()

        verify(listener).onOptionSelect(creditCard)
    }

    @Test
    fun `WHEN the header is clicked THEN view is expanded or collapsed`() {
        creditCardSelectBar.showPrompt(listOf(creditCard))

        creditCardSelectBar.findViewById<AppCompatTextView>(R.id.select_credit_card_header).performClick()

        assertTrue(creditCardSelectBar.findViewById<RecyclerView>(R.id.credit_cards_list).isVisible)
        assertTrue(creditCardSelectBar.findViewById<AppCompatTextView>(R.id.manage_credit_cards).isVisible)

        creditCardSelectBar.findViewById<AppCompatTextView>(R.id.select_credit_card_header).performClick()

        assertFalse(creditCardSelectBar.findViewById<RecyclerView>(R.id.credit_cards_list).isVisible)
        assertFalse(creditCardSelectBar.findViewById<AppCompatTextView>(R.id.manage_credit_cards).isVisible)
    }
}
