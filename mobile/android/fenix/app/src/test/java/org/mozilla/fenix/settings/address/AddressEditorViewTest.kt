/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.address

import android.view.LayoutInflater
import android.view.View
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.FragmentAddressEditorBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.address.interactor.AddressEditorInteractor
import org.mozilla.fenix.settings.address.view.AddressEditorView

@RunWith(FenixRobolectricTestRunner::class)
class AddressEditorViewTest {

    private lateinit var view: View
    private lateinit var interactor: AddressEditorInteractor
    private lateinit var addressEditorView: AddressEditorView
    private lateinit var binding: FragmentAddressEditorBinding

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext).inflate(R.layout.fragment_address_editor, null)
        binding = FragmentAddressEditorBinding.bind(view)
        interactor = mockk(relaxed = true)

        addressEditorView = spyk(AddressEditorView(binding, interactor))
    }

    @Test
    fun `WHEN the cancel button is clicked THEN interactor is called`() {
        addressEditorView.bind()

        binding.cancelButton.performClick()

        verify { interactor.onCancelButtonClicked() }
    }
}
