/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.address.controller

import androidx.navigation.NavController
import io.mockk.coVerify
import io.mockk.coVerifySequence
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.concept.storage.Address
import mozilla.components.concept.storage.UpdatableAddressFields
import mozilla.components.service.sync.autofill.AutofillCreditCardsAddressesStorage
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Before
import org.junit.Rule
import org.junit.Test

class DefaultAddressEditorControllerTest {

    private val storage: AutofillCreditCardsAddressesStorage = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var controller: DefaultAddressEditorController

    @Before
    fun setup() {
        controller = spyk(
            DefaultAddressEditorController(
                storage = storage,
                lifecycleScope = coroutinesTestRule.scope,
                navController = navController,
            ),
        )
    }

    @Test
    fun `WHEN cancel button is clicked THEN pop the NavController back stack`() {
        controller.handleCancelButtonClicked()

        verify {
            navController.popBackStack()
        }
    }

    @Test
    fun `GIVEN a new address record WHEN save address is called THEN save the new address record to storage`() = runTestOnMain {
        val addressFields = UpdatableAddressFields(
            name = "John Smith",
            organization = "Mozilla",
            streetAddress = "123 Sesame Street",
            addressLevel3 = "",
            addressLevel2 = "",
            addressLevel1 = "",
            postalCode = "90210",
            country = "US",
            tel = "+1 519 555-5555",
            email = "foo@bar.com",
        )

        controller.handleSaveAddress(addressFields)

        coVerify {
            storage.addAddress(addressFields)
            navController.popBackStack()
        }
    }

    @Test
    fun `GIVEN an existing address record WHEN save address is called THEN update the address record to storage`() = runTestOnMain {
        val address: Address = mockk()
        val addressFields: UpdatableAddressFields = mockk()
        every { address.guid } returns "123"

        controller.handleUpdateAddress(address.guid, addressFields)

        coVerifySequence {
            storage.updateAddress("123", addressFields)
            navController.popBackStack()
        }
    }
}
