/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.LayoutInflater
import io.mockk.every
import io.mockk.mockk
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Onboarding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.databinding.OnboardingToolbarPositionPickerBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class OnboardingToolbarPositionPickerViewHolderTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private lateinit var binding: OnboardingToolbarPositionPickerBinding
    private lateinit var settings: Settings

    @Before
    fun setup() {
        binding = OnboardingToolbarPositionPickerBinding.inflate(LayoutInflater.from(testContext))
        settings = mockk(relaxed = true)
        every { testContext.components.settings } returns settings
        every { testContext.components.analytics } returns mockk(relaxed = true)
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

    @Test
    fun `WHEN the top radio button is clicked THEN the proper event is recorded`() {
        every { settings.toolbarPosition } returns ToolbarPosition.BOTTOM
        OnboardingToolbarPositionPickerViewHolder(binding.root)

        binding.toolbarTopImage.performClick()

        assertNotNull(Onboarding.prefToggledToolbarPosition.testGetValue())
        assertEquals(
            OnboardingToolbarPositionPickerViewHolder.Companion.Position.TOP.name,
            Onboarding.prefToggledToolbarPosition.testGetValue()!!
                .last().extra?.get("position")
        )
    }

    @Test
    fun `WHEN the bottom radio button is clicked THEN the proper event is recorded`() {
        every { settings.toolbarPosition } returns ToolbarPosition.TOP
        OnboardingToolbarPositionPickerViewHolder(binding.root)

        binding.toolbarBottomImage.performClick()

        assertNotNull(Onboarding.prefToggledToolbarPosition.testGetValue())
        assertEquals(
            OnboardingToolbarPositionPickerViewHolder.Companion.Position.BOTTOM.name,
            Onboarding.prefToggledToolbarPosition.testGetValue()!!
                .last().extra?.get("position")
        )
    }
}
