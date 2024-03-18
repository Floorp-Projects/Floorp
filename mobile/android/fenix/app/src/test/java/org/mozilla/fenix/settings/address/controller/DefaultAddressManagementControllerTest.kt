/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.address.controller

import androidx.navigation.NavController
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.concept.storage.Address
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.settings.address.AddressManagementFragmentDirections

class DefaultAddressManagementControllerTest {

    private val navController: NavController = mockk(relaxed = true)

    private lateinit var controller: AddressManagementController

    @Before
    fun setup() {
        controller = spyk(
            DefaultAddressManagementController(
                navController = navController,
            ),
        )
    }

    @Test
    fun `WHEN an address is selected THEN navigate to the address editor`() {
        val address: Address = mockk(relaxed = true)

        controller.handleAddressClicked(address)

        verify {
            navController.navigate(
                AddressManagementFragmentDirections
                    .actionAddressManagementFragmentToAddressEditorFragment(
                        address = address,
                    ),
            )
        }
    }

    @Test
    fun `WHEN the add address button is clicked THEN navigate to the address editor`() {
        controller.handleAddAddressButtonClicked()

        verify {
            navController.navigate(
                AddressManagementFragmentDirections
                    .actionAddressManagementFragmentToAddressEditorFragment(),
            )
        }
    }
}
