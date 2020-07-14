/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.content.res.Resources
import android.view.LayoutInflater
import android.view.View
import io.mockk.every
import io.mockk.mockkStatic
import io.mockk.unmockkStatic
import kotlinx.android.synthetic.main.onboarding_tracking_protection.view.*
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class OnboardingTrackingProtectionViewHolderTest {

    private lateinit var view: View

    @Before
    fun setup() {
        mockkStatic("mozilla.components.support.ktx.android.content.res.ThemeKt")
        view = LayoutInflater.from(testContext)
            .inflate(OnboardingTrackingProtectionViewHolder.LAYOUT_ID, null)

        every {
            any<Resources.Theme>().resolveAttribute(R.attr.onboardingSelected)
        } returns R.color.onboarding_illustration_selected_normal_theme
    }

    @After
    fun teardown() {
        unmockkStatic("mozilla.components.support.ktx.android.content.res.ThemeKt")
    }

    @Test
    fun `sets description text`() {
        OnboardingTrackingProtectionViewHolder(view)

        assertEquals(
            "Privacy and security settings block trackers, malware, and companies that follow you.",
            view.description_text.text
        )
    }
}
