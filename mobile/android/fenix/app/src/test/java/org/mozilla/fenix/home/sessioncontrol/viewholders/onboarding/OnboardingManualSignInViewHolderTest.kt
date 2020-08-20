/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.navigation.NavController
import androidx.navigation.Navigation
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
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
import org.mozilla.fenix.onboarding.OnboardingInteractor

@RunWith(FenixRobolectricTestRunner::class)
class OnboardingManualSignInViewHolderTest {

    private lateinit var view: View
    private lateinit var navController: NavController
    private lateinit var interactor: OnboardingInteractor
    private lateinit var itemView: ViewGroup

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext)
            .inflate(OnboardingManualSignInViewHolder.LAYOUT_ID, null)
        navController = mockk(relaxed = true)
        interactor = mockk(relaxUnitFun = true)
        itemView = mockk(relaxed = true)

        mockkStatic(Navigation::class)
        every { itemView.context } returns testContext
        every { interactor.onLearnMoreClicked() } just Runs
        every { Navigation.findNavController(view) } returns navController
    }

    @After
    fun teardown() {
        unmockkStatic(Navigation::class)
    }

    @Test
    fun `bind header text`() {
        OnboardingManualSignInViewHolder(view).bind()

        assertEquals(
            "Start syncing bookmarks, passwords, and more with your Firefox account.",
            view.header_text.text
        )
    }

    @Test
    fun `navigate on click`() {
        OnboardingManualSignInViewHolder(view)
        view.fxa_sign_in_button.performClick()

        verify { navController.navigate(HomeFragmentDirections.actionGlobalTurnOnSync()) }
    }
}
