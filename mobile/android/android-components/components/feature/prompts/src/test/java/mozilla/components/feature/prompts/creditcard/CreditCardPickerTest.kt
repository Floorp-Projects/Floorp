/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.creditcard

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.storage.CreditCardEntry
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class CreditCardPickerTest {

    private lateinit var store: BrowserStore
    private lateinit var state: BrowserState
    private lateinit var creditCardPicker: CreditCardPicker
    private lateinit var creditCardSelectBar: CreditCardSelectBar

    private val creditCard = CreditCardEntry(
        guid = "1",
        name = "Banana Apple",
        number = "4111111111111110",
        expiryMonth = "5",
        expiryYear = "2030",
        cardType = "",
    )
    var onDismissCalled = false
    var confirmedCreditCard: CreditCardEntry? = null
    private val promptRequest = PromptRequest.SelectCreditCard(
        creditCards = listOf(creditCard),
        onDismiss = { onDismissCalled = true },
        onConfirm = { confirmedCreditCard = it },
    )

    var manageCreditCardsCalled = false
    var selectCreditCardCalled = false
    private val manageCreditCardsCallback: () -> Unit = { manageCreditCardsCalled = true }
    private val selectCreditCardCallback: () -> Unit = { selectCreditCardCalled = true }

    @Before
    fun setup() {
        store = mock()
        state = mock()
        creditCardSelectBar = mock()
        creditCardPicker = CreditCardPicker(
            store = store,
            creditCardSelectBar = creditCardSelectBar,
            manageCreditCardsCallback = manageCreditCardsCallback,
            selectCreditCardCallback = selectCreditCardCallback,
        )

        whenever(store.state).thenReturn(state)
    }

    @Test
    fun `WHEN onOptionSelect is called with a credit card THEN selectCreditCardCallback is invoked and prompt is hidden`() {
        assertNull(creditCardPicker.selectedCreditCard)

        setupSessionState(promptRequest)

        creditCardPicker.onOptionSelect(creditCard)

        verify(creditCardSelectBar).hidePrompt()

        assertTrue(selectCreditCardCalled)
        assertEquals(creditCard, creditCardPicker.selectedCreditCard)
    }

    @Test
    fun `WHEN onManageOptions is called THEN manageCreditCardsCallback is invoked and prompt is hidden`() {
        setupSessionState(promptRequest)

        creditCardPicker.onManageOptions()

        verify(creditCardSelectBar).hidePrompt()

        assertTrue(manageCreditCardsCalled)
        assertTrue(onDismissCalled)
    }

    @Test
    fun `GIVEN a prompt request WHEN handleSelectCreditCardRequest is called THEN the prompt is shown with the provided request credit cards`() {
        creditCardPicker.handleSelectCreditCardRequest(promptRequest)

        verify(creditCardSelectBar).showPrompt(promptRequest.creditCards)
    }

    @Test
    fun `GIVEN a custom tab and a prompt request WHEN handleSelectCreditCardRequest is called THEN the prompt is shown with the provided request credit cards`() {
        val customTabContent: ContentState = mock()
        val customTab = CustomTabSessionState("custom-tab", customTabContent, mock(), mock())

        whenever(customTabContent.promptRequests).thenReturn(listOf(promptRequest))
        whenever(state.customTabs).thenReturn(listOf(customTab))

        creditCardPicker.handleSelectCreditCardRequest(promptRequest)

        verify(creditCardSelectBar).showPrompt(promptRequest.creditCards)
    }

    @Test
    fun `GIVEN a selected credit card WHEN onAuthSuccess is called THEN the confirmed credit card is received`() {
        assertNull(creditCardPicker.selectedCreditCard)

        setupSessionState(promptRequest)

        creditCardPicker.onOptionSelect(creditCard)
        creditCardPicker.onAuthSuccess()

        assertEquals(creditCard, confirmedCreditCard)
        assertNull(creditCardPicker.selectedCreditCard)
    }

    @Test
    fun `GIVEN a selected credit card WHEN onAuthFailure is called THEN the prompt request is dismissed`() {
        assertNull(creditCardPicker.selectedCreditCard)

        setupSessionState(promptRequest)

        creditCardPicker.onOptionSelect(creditCard)
        creditCardPicker.onAuthFailure()

        assertNull(creditCardPicker.selectedCreditCard)
        assertTrue(onDismissCalled)
    }

    private fun setupSessionState(request: PromptRequest? = null): TabSessionState {
        val promptRequest: PromptRequest = request ?: mock()
        val content: ContentState = mock()

        whenever(content.promptRequests).thenReturn(listOf(promptRequest))

        val selected = TabSessionState("browser-tab", content, mock(), mock())

        whenever(state.selectedTabId).thenReturn(selected.id)
        whenever(state.tabs).thenReturn(listOf(selected))

        return selected
    }
}
