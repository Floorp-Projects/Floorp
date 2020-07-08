/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.LayoutInflater
import android.view.View
import androidx.navigation.NavController
import androidx.navigation.Navigation
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.unmockkStatic
import io.mockk.verify
import kotlinx.android.synthetic.main.onboarding_manual_signin.view.*
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.HomeFragmentDirections

@RunWith(FenixRobolectricTestRunner::class)
class OnboardingManualSignInViewHolderTest {

    private lateinit var view: View
    private lateinit var navController: NavController

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext)
            .inflate(OnboardingManualSignInViewHolder.LAYOUT_ID, null)
        navController = mockk(relaxed = true)

        mockkStatic(Navigation::class)
        every { Navigation.findNavController(view) } returns navController
    }

    @After
    fun teardown() {
        unmockkStatic(Navigation::class)
    }

    @Test
    fun `bind header text`() {
        OnboardingManualSignInViewHolder(view).bind()

        assertEquals("Get the most out of Firefox Preview.", view.header_text.text)
    }

    @Test
    fun `navigate on click`() {
        OnboardingManualSignInViewHolder(view)
        view.turn_on_sync_button.performClick()

        verify { navController.navigate(HomeFragmentDirections.actionGlobalTurnOnSync()) }
    }
}
