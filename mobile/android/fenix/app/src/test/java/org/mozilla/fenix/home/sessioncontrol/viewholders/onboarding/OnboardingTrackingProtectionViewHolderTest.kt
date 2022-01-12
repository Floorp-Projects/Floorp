/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.LayoutInflater
import androidx.appcompat.view.ContextThemeWrapper
import io.mockk.every
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.OnboardingTrackingProtectionBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class OnboardingTrackingProtectionViewHolderTest {

    private lateinit var binding: OnboardingTrackingProtectionBinding

    @Before
    fun setup() {
        val context = ContextThemeWrapper(testContext, R.style.NormalTheme)
        binding = OnboardingTrackingProtectionBinding.inflate(LayoutInflater.from(context))
    }

    @Test
    fun `sets description text`() {
        every { testContext.components.settings } returns Settings(testContext)
        OnboardingTrackingProtectionViewHolder(binding.root)

        val appName = testContext.getString(R.string.app_name)
        val string =
            testContext.getString(R.string.onboarding_tracking_protection_description_4, appName)
        assertEquals(string, binding.descriptionText.text)
    }
}
