/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.navigation.NavController
import androidx.navigation.Navigation
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.unmockkStatic
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.After
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Onboarding
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.OnboardingManualSigninBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.HomeFragmentDirections

@RunWith(FenixRobolectricTestRunner::class)
class OnboardingManualSignInViewHolderTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private lateinit var binding: OnboardingManualSigninBinding
    private lateinit var navController: NavController
    private lateinit var itemView: ViewGroup

    @Before
    fun setup() {
        binding = OnboardingManualSigninBinding.inflate(LayoutInflater.from(testContext))
        navController = mockk(relaxed = true)
        itemView = mockk(relaxed = true)

        mockkStatic(Navigation::class)
        every { itemView.context } returns testContext
        every { Navigation.findNavController(binding.root) } returns navController
    }

    @After
    fun teardown() {
        unmockkStatic(Navigation::class)
    }

    @Test
    fun `bind header text`() {
        OnboardingManualSignInViewHolder(binding.root).bind()
        val string = testContext.getString(R.string.onboarding_account_sign_in_header_1)
        assertEquals(
            string,
            binding.headerText.text
        )
    }

    @Test
    fun `navigate on click`() {
        every { testContext.components.analytics } returns mockk(relaxed = true)
        OnboardingManualSignInViewHolder(binding.root)
        binding.fxaSignInButton.performClick()

        verify { navController.navigate(HomeFragmentDirections.actionGlobalTurnOnSync()) }
        // Check if the event was recorded
        Assert.assertTrue(Onboarding.fxaManualSignin.testHasValue())
        assertEquals(1, Onboarding.fxaManualSignin.testGetValue().size)
        Assert.assertNull(Onboarding.fxaManualSignin.testGetValue().single().extra)
    }
}
