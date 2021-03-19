/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.LayoutInflater
import android.view.View
import androidx.appcompat.view.ContextThemeWrapper
import kotlinx.android.synthetic.main.onboarding_tracking_protection.view.*
import mozilla.components.support.test.robolectric.testContext
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
        val context = ContextThemeWrapper(testContext, R.style.NormalTheme)
        view = LayoutInflater.from(context)
            .inflate(OnboardingTrackingProtectionViewHolder.LAYOUT_ID, null)
    }

    @Test
    fun `sets description text`() {
        OnboardingTrackingProtectionViewHolder(view)

        val string = testContext.getString(R.string.onboarding_tracking_protection_description_3)
        assertEquals(string, view.description_text.text)
    }
}
