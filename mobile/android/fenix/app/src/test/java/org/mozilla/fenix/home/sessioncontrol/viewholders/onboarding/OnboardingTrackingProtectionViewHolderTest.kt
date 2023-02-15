/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.content.Context
import android.view.LayoutInflater
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.OnboardingTrackingProtectionBinding
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class OnboardingTrackingProtectionViewHolderTest {
    @Test
    fun `GIVEN the TCP feature is public WHEN this ViewHolder is initialized THEN set an appropriate description`() {
        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().settings() } returns mockk(relaxed = true) {
                every { enabledTotalCookieProtectionCFR } returns true
            }
            val expectedDescription = testContext.getString(R.string.onboarding_tracking_protection_description)

            val binding = OnboardingTrackingProtectionBinding.inflate(LayoutInflater.from(testContext))
            OnboardingTrackingProtectionViewHolder(binding.root)

            assertEquals(expectedDescription, binding.descriptionText.text)
        }
    }

    @Test
    fun `GIVEN the TCP feature is not public WHEN this ViewHolder is initialized THEN set an appropriate description`() {
        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().settings() } returns mockk(relaxed = true) {
                every { enabledTotalCookieProtectionCFR } returns false
            }
            val expectedDescription = testContext.getString(
                R.string.onboarding_tracking_protection_description_old,
                testContext.getString(R.string.app_name),
            )

            val binding = OnboardingTrackingProtectionBinding.inflate(LayoutInflater.from(testContext))
            OnboardingTrackingProtectionViewHolder(binding.root)

            assertEquals(expectedDescription, binding.descriptionText.text)
        }
    }
}
