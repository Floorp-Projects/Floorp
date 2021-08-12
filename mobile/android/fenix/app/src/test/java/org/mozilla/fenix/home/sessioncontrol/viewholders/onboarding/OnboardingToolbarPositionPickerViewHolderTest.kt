/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.LayoutInflater
import io.mockk.every
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.databinding.OnboardingToolbarPositionPickerBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class OnboardingToolbarPositionPickerViewHolderTest {

    private lateinit var binding: OnboardingToolbarPositionPickerBinding
    private lateinit var settings: Settings

    @Before
    fun setup() {
        val components = testContext.components
        binding = OnboardingToolbarPositionPickerBinding.inflate(LayoutInflater.from(testContext))
        settings = components.settings
    }

    @Test
    fun `bottom illustration should select corresponding radio button`() {
        every { settings.toolbarPosition } returns ToolbarPosition.TOP
        OnboardingToolbarPositionPickerViewHolder(binding.root)
        assertTrue(binding.toolbarTopRadioButton.isChecked)
        assertFalse(binding.toolbarBottomRadioButton.isChecked)

        binding.toolbarBottomImage.performClick()
        assertFalse(binding.toolbarTopRadioButton.isChecked)
        assertTrue(binding.toolbarBottomRadioButton.isChecked)
    }

    @Test
    fun `top illustration should select corresponding radio button`() {
        every { settings.toolbarPosition } returns ToolbarPosition.BOTTOM
        OnboardingToolbarPositionPickerViewHolder(binding.root)
        assertFalse(binding.toolbarTopRadioButton.isChecked)
        assertTrue(binding.toolbarBottomRadioButton.isChecked)

        binding.toolbarTopImage.performClick()
        assertTrue(binding.toolbarTopRadioButton.isChecked)
        assertFalse(binding.toolbarBottomRadioButton.isChecked)
    }
}
