/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.address.interactor

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.storage.Address
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.settings.address.controller.AddressManagementController

class DefaultAddressManagementInteractorTest {

    private val controller: AddressManagementController = mockk(relaxed = true)

    private lateinit var interactor: AddressManagementInteractor

    @Before
    fun setup() {
        interactor = DefaultAddressManagementInteractor(controller)
    }

    @Test
    fun `WHEN an address is selected THEN forward to controller handler`() {
        val address: Address = mockk(relaxed = true)

        interactor.onSelectAddress(address)

        verify { controller.handleAddressClicked(address) }
    }

    @Test
    fun `WHEN add address button is clicked THEN forward to controller handler`() {
        interactor.onAddAddressButtonClick()

        verify { controller.handleAddAddressButtonClicked() }
    }
}
