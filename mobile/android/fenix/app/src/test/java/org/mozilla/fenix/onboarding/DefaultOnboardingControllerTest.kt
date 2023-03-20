/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import androidx.navigation.NavController
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.impl.annotations.RelaxedMockK
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.slot
import io.mockk.unmockkObject
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
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.onboarding.controller.DefaultOnboardingController
import org.mozilla.fenix.settings.SupportUtils

@RunWith(FenixRobolectricTestRunner::class)
class DefaultOnboardingControllerTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @RelaxedMockK
    private lateinit var activity: HomeActivity

    @MockK(relaxUnitFun = true)
    private lateinit var navController: NavController

    @RelaxedMockK
    private lateinit var onboarding: FenixOnboarding

    private lateinit var controller: DefaultOnboardingController

    @Before
    fun setup() {
        MockKAnnotations.init(this)

        controller = DefaultOnboardingController(
            activity = activity,
            navController = navController,
            onboarding = onboarding,
        )
    }

    @Test
    fun `GIVEN focus on address bar is true WHEN onboarding is finished THEN navigate to home and log search bar tapped`() {
        assertNull(Events.searchBarTapped.testGetValue())

        val focusOnAddressBar = true

        controller.handleFinishOnboarding(focusOnAddressBar)

        assertNotNull(Events.searchBarTapped.testGetValue())

        val recordedEvents = Events.searchBarTapped.testGetValue()!!
        assertEquals(1, recordedEvents.size)
        assertEquals("HOME", recordedEvents.single().extra?.getValue("source"))

        verify {
            onboarding.finish()

            navController.navigate(
                OnboardingFragmentDirections.actionHome(focusOnAddressBar = focusOnAddressBar),
            )
        }
    }

    @Test
    fun `GIVEN focus on address bar is false WHEN onboarding is finished THEN navigate to home`() {
        assertNull(Events.searchBarTapped.testGetValue())

        val focusOnAddressBar = false

        controller.handleFinishOnboarding(focusOnAddressBar)

        assertNull(Events.searchBarTapped.testGetValue())

        verify {
            onboarding.finish()

            navController.navigate(
                OnboardingFragmentDirections.actionHome(focusOnAddressBar = focusOnAddressBar),
            )
        }
    }

    @Test
    fun `WHEN read privacy notice is clicked THEN open the privacy notice support page`() {
        mockkObject(SupportUtils)
        val urlCaptor = slot<String>()
        every { SupportUtils.createCustomTabIntent(any(), capture(urlCaptor)) } returns mockk()

        controller.handleReadPrivacyNoticeClicked()

        verify {
            activity.startActivity(
                any(),
            )
        }
        assertEquals(
            SupportUtils.getMozillaPageUrl(SupportUtils.MozillaPage.PRIVATE_NOTICE),
            urlCaptor.captured,
        )
        unmockkObject(SupportUtils)
    }
}
