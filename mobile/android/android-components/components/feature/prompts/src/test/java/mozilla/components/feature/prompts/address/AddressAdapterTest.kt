/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.address

import android.view.LayoutInflater
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.storage.Address
import mozilla.components.feature.prompts.R
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class AddressAdapterTest {

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

    @Test
    fun testAddressDiffCallback() {
        val address2 = address.copy()

        assertTrue(
            AddressDiffCallback.areItemsTheSame(address, address2)
        )
        assertTrue(
            AddressDiffCallback.areContentsTheSame(address, address2)
        )

        val address3 = address.copy(guid = "2")

        assertFalse(
            AddressDiffCallback.areItemsTheSame(address, address3)
        )
        assertFalse(
            AddressDiffCallback.areItemsTheSame(address, address3)
        )
    }

    @Test
    fun `WHEN an address is bound to the adapter THEN set the address display name`() {
        val view =
            LayoutInflater.from(testContext)
                .inflate(R.layout.mozac_feature_prompts_address_list_item, null)
        val addressName: TextView = view.findViewById(R.id.address_name)

        AddressViewHolder(view, onAddressSelected = {}).bind(address)

        assertEquals(address.addressLabel, addressName.text)
    }

    @Test
    fun `WHEN an address item is clicked THEN call the onAddressSelected callback`() {
        var addressSelected = false
        val view =
            LayoutInflater.from(testContext)
                .inflate(R.layout.mozac_feature_prompts_address_list_item, null)
        val onAddressSelect: (Address) -> Unit = { addressSelected = true }

        AddressViewHolder(view, onAddressSelect).bind(address)
        view.performClick()

        assertTrue(addressSelected)
    }
}
