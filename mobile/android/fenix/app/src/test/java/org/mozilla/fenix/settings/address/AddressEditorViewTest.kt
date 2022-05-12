/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.address

import android.view.LayoutInflater
import android.view.View
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.search.RegionState
import mozilla.components.concept.storage.Address
import mozilla.components.concept.storage.UpdatableAddressFields
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Addresses
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.FragmentAddressEditorBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.address.interactor.AddressEditorInteractor
import org.mozilla.fenix.settings.address.view.AddressEditorView

@RunWith(FenixRobolectricTestRunner::class) // For gleanTestRule
class AddressEditorViewTest {

    private lateinit var view: View
    private lateinit var interactor: AddressEditorInteractor
    private lateinit var addressEditorView: AddressEditorView
    private lateinit var binding: FragmentAddressEditorBinding
    private lateinit var address: Address

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext).inflate(R.layout.fragment_address_editor, null)
        binding = FragmentAddressEditorBinding.bind(view)
        interactor = mockk(relaxed = true)
        address = mockk(relaxed = true)
        every { address.guid } returns "123"

        addressEditorView = spyk(AddressEditorView(binding, interactor))
    }

    @Test
    fun `GIVEN an existing address WHEN the save button is clicked THEN interactor updates address`() {
        val country = AddressUtils.countries["US"]!!
        val address = generateAddress(country = country.countryCode, addressLevel1 = country.subregions[0])
        val addressEditorView = AddressEditorView(
            binding = binding,
            interactor = interactor,
            address = address,
        )

        addressEditorView.bind()
        addressEditorView.saveAddress()

        val expected = UpdatableAddressFields(
            givenName = address.givenName,
            additionalName = address.additionalName,
            familyName = address.familyName,
            organization = "",
            streetAddress = address.streetAddress,
            addressLevel3 = "",
            addressLevel2 = "",
            addressLevel1 = address.addressLevel1,
            postalCode = address.postalCode,
            country = address.country,
            tel = address.tel,
            email = address.email,
        )
        verify { interactor.onUpdateAddress(address.guid, expected) }
    }

    @Test
    fun `GIVEN a new address WHEN the save button is clicked THEN interactor saves new address`() {
        val addressEditorView = AddressEditorView(
            binding = binding,
            interactor = interactor,
        )

        addressEditorView.bind()
        addressEditorView.saveAddress()

        val expected = UpdatableAddressFields(
            givenName = "",
            additionalName = "",
            familyName = "",
            organization = "",
            streetAddress = "",
            addressLevel3 = "",
            addressLevel2 = "",
            addressLevel1 = "Alabama",
            postalCode = "",
            country = "US",
            tel = "",
            email = "",
        )
        verify { interactor.onSaveAddress(expected) }
    }

    @Test
    fun `WHEN the cancel button is clicked THEN interactor is called`() {
        addressEditorView.bind()

        binding.cancelButton.performClick()

        verify { interactor.onCancelButtonClicked() }
    }

    @Test
    fun `GIVEN an existing address WHEN editor is opened THEN the form fields are correctly mapped to the address fields`() {
        val address = generateAddress()

        val addressEditorView = spyk(
            AddressEditorView(
                binding = binding,
                interactor = interactor,
                address = address,
            )
        )
        addressEditorView.bind()

        assertEquals("PostalCode", binding.zipInput.text.toString())
        assertEquals(address.addressLevel1, binding.subregionDropDown.selectedItem.toString())
        assertEquals("City", binding.cityInput.text.toString())
        assertEquals("Street", binding.streetAddressInput.text.toString())
        assertEquals("Family", binding.lastNameInput.text.toString())
        assertEquals("Given", binding.firstNameInput.text.toString())
        assertEquals("Additional", binding.middleNameInput.text.toString())
        assertEquals("email@mozilla.com", binding.emailInput.text.toString())
        assertEquals("Telephone", binding.phoneInput.text.toString())
    }

    @Test
    fun `GIVEN an existing address WHEN editor is opened THEN the delete address button is visible`() = runBlocking {
        val addressEditorView = spyk(
            AddressEditorView(
                binding = binding,
                interactor = interactor,
                address = address,
            )
        )
        addressEditorView.bind()

        assertEquals(View.VISIBLE, binding.deleteButton.visibility)
    }

    @Test
    fun `GIVEN an existing address WHEN the delete address button is clicked THEN confirm delete dialog is shown`() = runBlocking {
        val addressEditorView = spyk(
            AddressEditorView(
                binding = binding,
                interactor = interactor,
                address = address,
            )
        )
        addressEditorView.bind()

        binding.deleteButton.performClick()

        verify { addressEditorView.showConfirmDeleteAddressDialog(view.context, "123") }
    }

    @Test
    fun `GIVEN existing address with correct subregion and country WHEN subregion dropdown is bound THEN adapter sets subregion dropdown to address`() {
        val address = generateAddress(country = "US", addressLevel1 = "Oregon")

        val addressEditorView = AddressEditorView(
            binding = binding,
            interactor = interactor,
            address = address,
        )
        addressEditorView.bind()

        assertEquals("Oregon", binding.subregionDropDown.selectedItem.toString())
    }

    @Test
    fun `GIVEN existing address subregion outside of country WHEN subregion dropdown is bound THEN dropdown defaults to first subregion entry for country`() {
        val address = generateAddress(country = "CA", addressLevel1 = "Alabama")

        val addressEditorView = AddressEditorView(
            binding = binding,
            interactor = interactor,
            address = address,
        )
        addressEditorView.bind()

        assertEquals("Alberta", binding.subregionDropDown.selectedItem.toString())
    }

    @Test
    fun `GIVEN no existing address WHEN subregion dropdown is bound THEN dropdown defaults to first subregion of default country`() {
        val addressEditorView = AddressEditorView(
            binding = binding,
            interactor = interactor,
        )
        addressEditorView.bind()

        assertEquals("Alabama", binding.subregionDropDown.selectedItem.toString())
    }

    @Test
    fun `WHEN country is changed THEN available subregions are updated`() {
        val addressEditorView = AddressEditorView(
            binding = binding,
            interactor = interactor,
        )
        addressEditorView.bind()

        assertEquals("Alabama", binding.subregionDropDown.selectedItem.toString())
        binding.countryDropDown.setSelection(0)
        assertNotEquals("Alabama", binding.subregionDropDown.selectedItem.toString())
    }

    @Test
    fun `GIVEN existing address not in available countries WHEN view is bound THEN country and subregion dropdowns are set to default `() {
        val address = generateAddress(country = "I AM NOT A COUNTRY", addressLevel1 = "I AM NOT A STATE")
        val addressEditorView = AddressEditorView(
            binding = binding,
            interactor = interactor,
            address = address,
        )
        addressEditorView.bind()

        assertEquals("United States", binding.countryDropDown.selectedItem.toString())
        assertEquals("Alabama", binding.subregionDropDown.selectedItem.toString())
    }

    @Test
    fun `GIVEN existing address WHEN country dropdown is bound THEN adapter sets country dropdown to address`() {
        val addressEditorView = spyk(
            AddressEditorView(
                binding = binding,
                interactor = interactor,
                address = generateAddress(country = "CA"),
            )
        )
        addressEditorView.bind()

        assertEquals(AddressUtils.countries["CA"]?.displayName, binding.countryDropDown.selectedItem.toString())
    }

    @Test
    fun `GIVEN existing address and region not in supported countries WHEN country dropdown is bound THEN adapter sets dropdown to lower priority`() {
        val addressEditorView = spyk(
            AddressEditorView(
                binding = binding,
                interactor = interactor,
                region = RegionState.Default,
                address = generateAddress(country = "XX"),
            )
        )
        addressEditorView.bind()

        assertEquals(AddressUtils.countries[DEFAULT_COUNTRY]!!.displayName, binding.countryDropDown.selectedItem.toString())
    }

    @Test
    fun `GIVEN search region and no address WHEN country dropdown is bound THEN adapter sets country dropdown to home region`() {
        val addressEditorView = AddressEditorView(
            binding = binding,
            interactor = interactor,
            region = RegionState("CA", "US"),
            address = null,
        )
        addressEditorView.bind()

        assertEquals(AddressUtils.countries["CA"]?.displayName, binding.countryDropDown.selectedItem.toString())
    }

    @Test
    fun `GIVEN search region not in supported countries WHEN country dropdown is bound THEN adapter sets dropdown to lower priority`() {
        val addressEditorView = AddressEditorView(
            binding = binding,
            interactor = interactor,
            region = RegionState.Default,
            address = null,
        )
        addressEditorView.bind()

        assertEquals(AddressUtils.countries[DEFAULT_COUNTRY]!!.displayName, binding.countryDropDown.selectedItem.toString())
    }

    @Test
    fun `GIVEN no address or search region WHEN country dropdown is bound THEN adapter sets dropdown to default`() {
        val addressEditorView = AddressEditorView(
            binding = binding,
            interactor = interactor,
            region = null,
            address = null,
        )
        addressEditorView.bind()

        assertEquals(AddressUtils.countries[DEFAULT_COUNTRY]!!.displayName, binding.countryDropDown.selectedItem.toString())
    }

    @Test
    fun `GIVEN an existing address WHEN the save button is clicked THEN proper metrics are recorded`() = runBlocking {
        assertNull(Addresses.updated.testGetValue())

        val addressEditorView = spyk(AddressEditorView(binding, interactor, address = address))
        addressEditorView.bind()

        binding.saveButton.performClick()

        assertNotNull(Addresses.updated.testGetValue())
    }

    @Test
    fun `GIVEN a new address WHEN the save button is clicked THEN proper metrics are recorded`() = runBlocking {
        assertNull(Addresses.saved.testGetValue())

        val addressEditorView = spyk(AddressEditorView(binding, interactor))
        addressEditorView.bind()

        binding.saveButton.performClick()

        assertNotNull(Addresses.saved.testGetValue())
    }

    private fun generateAddress(country: String = "US", addressLevel1: String = "Oregon") = Address(
        guid = "123",
        givenName = "Given",
        additionalName = "Additional",
        familyName = "Family",
        organization = "Organization",
        streetAddress = "Street",
        addressLevel3 = "Suburb",
        addressLevel2 = "City",
        addressLevel1 = addressLevel1,
        postalCode = "PostalCode",
        country = country,
        tel = "Telephone",
        email = "email@mozilla.com",
        timeCreated = 0L,
        timeLastUsed = 1L,
        timeLastModified = 1L,
        timesUsed = 2L
    )
}
