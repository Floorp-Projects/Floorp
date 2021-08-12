/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.LayoutInflater
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.OnboardingHeaderBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class OnboardingHeaderViewHolderTest {

    private lateinit var binding: OnboardingHeaderBinding

    @Before
    fun setup() {
        binding = OnboardingHeaderBinding.inflate(LayoutInflater.from(testContext))
    }

    @Test
    fun `bind header text`() {
        val appName = testContext.getString(R.string.app_name)
        val welcomeMessage = testContext.getString(R.string.onboarding_header, appName)

        OnboardingHeaderViewHolder(binding.root)

        assertEquals(welcomeMessage, binding.headerText.text)
    }
}
