/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.address

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.storage.Address
import mozilla.components.feature.prompts.facts.AddressAutofillDialogFacts
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.FactProcessor
import mozilla.components.support.base.facts.Facts
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class AddressPickerTest {

    private lateinit var store: BrowserStore
    private lateinit var state: BrowserState
    private lateinit var addressPicker: AddressPicker
    private lateinit var addressSelectBar: AddressSelectBar

    private val address = Address(
        guid = "1",
        givenName = "Location",
        additionalName = "Location",
        familyName = "Location",
        organization = "Mozilla",
        streetAddress = "1230 Main st",
        addressLevel3 = "Location3",
        addressLevel2 = "Location2",
        addressLevel1 = "Location1",
        postalCode = "90237",
        country = "USA",
        tel = "00",
        email = "email"
    )

    private var onDismissCalled = false
    private var confirmedAddress: Address? = null

    private val promptRequest = PromptRequest.SelectAddress(
        addresses = listOf(address),
        onDismiss = { onDismissCalled = true },
        onConfirm = { confirmedAddress = it }
    )

    @Before
    fun setup() {
        store = mock()
        state = mock()
        addressSelectBar = mock()
        addressPicker = AddressPicker(
            store = store,
            addressSelectBar = addressSelectBar
        )

        whenever(store.state).thenReturn(state)
    }

    @Test
    fun `WHEN onOptionSelect is called with an address THEN selectAddressCallback is invoked and prompt is hidden`() {
        val content: ContentState = mock()
        whenever(content.promptRequests).thenReturn(listOf(promptRequest))
        val selectedTab = TabSessionState("browser-tab", content, mock(), mock())
        whenever(state.selectedTabId).thenReturn(selectedTab.id)
        whenever(state.tabs).thenReturn(listOf(selectedTab))

        addressPicker.onOptionSelect(address)

        verify(addressSelectBar).hidePrompt()
        assertEquals(address, confirmedAddress)
    }

    @Test
    fun `GIVEN a prompt request WHEN handleSelectAddressRequest is called THEN the prompt is shown with the provided addresses`() {
        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(object : FactProcessor {
            override fun process(fact: Fact) {
                facts.add(fact)
            }
        })

        assertEquals(0, facts.size)

        addressPicker.handleSelectAddressRequest(promptRequest)

        assertEquals(1, facts.size)
        facts[0].apply {
            assertEquals(Component.FEATURE_PROMPTS, component)
            assertEquals(Action.INTERACTION, action)
            assertEquals(AddressAutofillDialogFacts.Items.AUTOFILL_ADDRESS_PROMPT_SHOWN, item)
        }

        verify(addressSelectBar).showPrompt(promptRequest.addresses)
    }

    @Test
    fun `GIVEN a custom tab and a prompt request WHEN handleSelectAddressRequest is called THEN the prompt is shown with the provided addresses`() {
        val customTabContent: ContentState = mock()
        val customTab = CustomTabSessionState("custom-tab", customTabContent, mock(), mock())
        whenever(customTabContent.promptRequests).thenReturn(listOf(promptRequest))
        whenever(state.customTabs).thenReturn(listOf(customTab))

        addressPicker.handleSelectAddressRequest(promptRequest)

        verify(addressSelectBar).showPrompt(promptRequest.addresses)
    }

    @Test
    fun `WHEN onManageOptions is called THEN onManageAddresses is invoked and prompt is hidden`() {
        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(object : FactProcessor {
            override fun process(fact: Fact) {
                facts.add(fact)
            }
        })

        assertEquals(0, facts.size)

        addressPicker.onManageOptions()

        assertEquals(1, facts.size)
        facts[0].apply {
            assertEquals(Component.FEATURE_PROMPTS, component)
            assertEquals(Action.INTERACTION, action)
            assertEquals(AddressAutofillDialogFacts.Items.AUTOFILL_ADDRESS_PROMPT_DISMISSED, item)
        }

        verify(addressSelectBar).hidePrompt()
    }
}
