/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.LayoutInflater
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Onboarding
import org.mozilla.fenix.databinding.OnboardingFinishBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.onboarding.interactor.OnboardingInteractor

@RunWith(FenixRobolectricTestRunner::class)
class OnboardingFinishViewHolderTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private lateinit var binding: OnboardingFinishBinding
    private lateinit var interactor: OnboardingInteractor

    @Before
    fun setup() {
        binding = OnboardingFinishBinding.inflate(LayoutInflater.from(testContext))
        interactor = mockk(relaxed = true)
    }

    @Test
    fun `call interactor on click`() {
        every { testContext.components.analytics } returns mockk(relaxed = true)
        OnboardingFinishViewHolder(binding.root, interactor)

        binding.finishButton.performClick()
        verify { interactor.onFinishOnboarding(focusOnAddressBar = true) }
        // Check if the event was recorded
        assertNotNull(Onboarding.finish.testGetValue())
        assertEquals(1, Onboarding.finish.testGetValue()!!.size)
        assertNull(Onboarding.finish.testGetValue()!!.single().extra)
    }
}
