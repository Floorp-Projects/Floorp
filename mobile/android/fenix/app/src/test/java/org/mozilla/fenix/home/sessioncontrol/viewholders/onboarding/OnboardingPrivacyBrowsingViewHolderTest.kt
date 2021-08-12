/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.LayoutInflater
import androidx.appcompat.view.ContextThemeWrapper
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.OnboardingPrivateBrowsingBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.sessioncontrol.OnboardingInteractor

@RunWith(FenixRobolectricTestRunner::class)
class OnboardingPrivacyBrowsingViewHolderTest {

    private lateinit var binding: OnboardingPrivateBrowsingBinding
    private lateinit var interactor: OnboardingInteractor

    @Before
    fun setup() {
        val context = ContextThemeWrapper(testContext, R.style.NormalTheme)
        binding = OnboardingPrivateBrowsingBinding.inflate(LayoutInflater.from(context))
        interactor = mockk(relaxed = true)
    }

    @Test
    fun `call interactor on click`() {
        OnboardingPrivateBrowsingViewHolder(binding.root, interactor)

        binding.openSettingsButton.performClick()
        verify { interactor.onOpenSettingsClicked() }
    }
}
