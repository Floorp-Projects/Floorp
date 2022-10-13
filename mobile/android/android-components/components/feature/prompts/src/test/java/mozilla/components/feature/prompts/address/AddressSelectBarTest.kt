/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.address

import android.widget.LinearLayout
import androidx.appcompat.widget.AppCompatTextView
import androidx.core.view.isVisible
import androidx.recyclerview.widget.RecyclerView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.storage.Address
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.concept.SelectablePromptView
import mozilla.components.feature.prompts.facts.AddressAutofillDialogFacts
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.FactProcessor
import mozilla.components.support.base.facts.Facts
import mozilla.components.support.test.ext.appCompatContext
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class AddressSelectBarTest {

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
        email = "email",
    )

    @Before
    fun setup() {
        addressSelectBar = AddressSelectBar(appCompatContext)
    }

    @Test
    fun `WHEN showPrompt is called THEN the select bar is shown`() {
        val addresses = listOf(address)

        addressSelectBar.showPrompt(addresses)

        assertTrue(addressSelectBar.isVisible)
    }

    @Test
    fun `WHEN hidePrompt is called THEN the select bar is hidden`() {
        assertTrue(addressSelectBar.isVisible)

        addressSelectBar.hidePrompt()

        assertFalse(addressSelectBar.isVisible)
    }

    @Test
    fun `WHEN the selectBar header is clicked two times THEN the list of addresses is shown, then hidden`() {
        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    facts.add(fact)
                }
            },
        )

        addressSelectBar.showPrompt(listOf(address))

        assertEquals(0, facts.size)

        addressSelectBar.findViewById<AppCompatTextView>(R.id.select_address_header).performClick()

        assertEquals(1, facts.size)
        facts[0].apply {
            assertEquals(Component.FEATURE_PROMPTS, component)
            assertEquals(Action.INTERACTION, action)
            assertEquals(AddressAutofillDialogFacts.Items.AUTOFILL_ADDRESS_PROMPT_EXPANDED, item)
        }

        assertTrue(addressSelectBar.findViewById<RecyclerView>(R.id.address_list).isVisible)

        addressSelectBar.findViewById<AppCompatTextView>(R.id.select_address_header).performClick()

        assertFalse(addressSelectBar.findViewById<RecyclerView>(R.id.address_list).isVisible)
    }

    @Test
    fun `GIVEN a listener WHEN an address is clicked THEN onOptionSelected is called`() {
        val listener: SelectablePromptView.Listener<Address> = mock()

        assertNull(addressSelectBar.listener)

        addressSelectBar.listener = listener

        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    facts.add(fact)
                }
            },
        )

        addressSelectBar.showPrompt(listOf(address))
        val adapter = addressSelectBar.findViewById<RecyclerView>(R.id.address_list).adapter as AddressAdapter
        val holder = adapter.onCreateViewHolder(LinearLayout(testContext), 0)
        adapter.bindViewHolder(holder, 0)

        assertEquals(0, facts.size)

        holder.itemView.performClick()

        assertEquals(1, facts.size)
        facts[0].apply {
            assertEquals(Component.FEATURE_PROMPTS, component)
            assertEquals(Action.INTERACTION, action)
            assertEquals(AddressAutofillDialogFacts.Items.AUTOFILL_ADDRESS_SUCCESS, item)
        }

        verify(listener).onOptionSelect(address)
    }
}
