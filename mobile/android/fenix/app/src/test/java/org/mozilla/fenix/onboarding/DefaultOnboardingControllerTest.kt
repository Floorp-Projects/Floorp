/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.slot
import io.mockk.unmockkObject
import io.mockk.verify
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.onboarding.controller.DefaultOnboardingController
import org.mozilla.fenix.settings.SupportUtils

class DefaultOnboardingControllerTest {

    private val activity: HomeActivity = mockk(relaxed = true)

    @Test
    fun handleStartBrowsingClicked() {
        var hideOnboardingInvoked = false
        createController(hideOnboarding = { hideOnboardingInvoked = true }).handleStartBrowsingClicked()

        assertTrue(hideOnboardingInvoked)
    }

    @Test
    fun handleReadPrivacyNoticeClicked() {
        mockkObject(SupportUtils)
        val urlCaptor = slot<String>()
        every { SupportUtils.createCustomTabIntent(any(), capture(urlCaptor)) } returns mockk()

        createController().handleReadPrivacyNoticeClicked()

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

    private fun createController(
        hideOnboarding: () -> Unit = {},
    ) = DefaultOnboardingController(
        activity = activity,
        hideOnboarding = hideOnboarding,
    )
}
