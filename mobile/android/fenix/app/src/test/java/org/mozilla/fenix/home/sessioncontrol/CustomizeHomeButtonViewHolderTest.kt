/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.view.LayoutInflater
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.CustomizeHomeListItemBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.sessioncontrol.viewholders.CustomizeHomeButtonViewHolder

@RunWith(FenixRobolectricTestRunner::class)
class CustomizeHomeButtonViewHolderTest {

    private lateinit var binding: CustomizeHomeListItemBinding
    private lateinit var interactor: CustomizeHomeIteractor

    @Before
    fun setup() {
        binding = CustomizeHomeListItemBinding.inflate(LayoutInflater.from(testContext))
        interactor = mockk(relaxed = true)
    }

    @Test
    fun `hide view and change setting on remove placeholder click`() {
        CustomizeHomeButtonViewHolder(binding.root, interactor)

        binding.customizeHome.performClick()

        verify {
            interactor.openCustomizeHomePage()
        }
    }
}
