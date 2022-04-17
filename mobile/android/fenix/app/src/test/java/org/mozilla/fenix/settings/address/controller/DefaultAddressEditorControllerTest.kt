/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.address.controller

import androidx.navigation.NavController
import io.mockk.coVerify
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.test.TestCoroutineScope
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.concept.storage.UpdatableAddressFields
import mozilla.components.service.sync.autofill.AutofillCreditCardsAddressesStorage
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test

class DefaultAddressEditorControllerTest {

    private val storage: AutofillCreditCardsAddressesStorage = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val testCoroutineScope = TestCoroutineScope()

    private lateinit var controller: DefaultAddressEditorController

    @Before
    fun setup() {
        controller = spyk(
            DefaultAddressEditorController(
                storage = storage,
                lifecycleScope = testCoroutineScope,
                navController = navController,
            )
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
    fun `GIVEN a new address record WHEN save address is called THEN save the new address record to storage`() = testCoroutineScope.runBlockingTest {
        val addressFields = UpdatableAddressFields(
            givenName = "John",
            additionalName = "",
            familyName = "Smith",
            organization = "Mozilla",
            streetAddress = "123 Sesame Street",
            addressLevel3 = "",
            addressLevel2 = "",
            addressLevel1 = "",
            postalCode = "90210",
            country = "US",
            tel = "+1 519 555-5555",
            email = "foo@bar.com"
        )

        controller.handleSaveAddress(addressFields)

        coVerify {
            storage.addAddress(addressFields)
            navController.popBackStack()
        }
    }
}
