/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.address.interactor

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.storage.UpdatableAddressFields
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.settings.address.controller.AddressEditorController

class DefaultAddressEditorInteractorTest {

    private val controller: AddressEditorController = mockk(relaxed = true)

    private lateinit var interactor: AddressEditorInteractor

    @Before
    fun setup() {
        interactor = DefaultAddressEditorInteractor(controller)
    }

    @Test
    fun `WHEN cancel button is clicked THEN forward to controller handler`() {
        interactor.onCancelButtonClicked()
        verify { controller.handleCancelButtonClicked() }
    }

    @Test
    fun `WHEN save button is clicked THEN forward to controller handler`() {
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

        interactor.onSaveAddress(addressFields)

        verify { controller.handleSaveAddress(addressFields) }
    }
}
