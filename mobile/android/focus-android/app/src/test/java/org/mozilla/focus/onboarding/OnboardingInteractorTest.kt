/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.onboarding

import androidx.preference.PreferenceManager
import androidx.test.core.app.ApplicationProvider
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.MockitoAnnotations
import org.mozilla.focus.fragment.onboarding.OnboardingFragment
import org.mozilla.focus.fragment.onboarding.OnboardingInteractor
import org.mozilla.focus.state.AppStore
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class OnboardingInteractorTest {

    @Mock
    private lateinit var appStore: AppStore

    @Before
    fun init() {
        MockitoAnnotations.openMocks(this)
    }

    @Test
    fun `given onboarding, when start browsing is pressed, then onboarding flag is true`() {
        OnboardingInteractor(appStore).finishOnboarding(ApplicationProvider.getApplicationContext(), "1")
        val prefManager =
            PreferenceManager.getDefaultSharedPreferences(ApplicationProvider.getApplicationContext())
        Assert.assertEquals(true, prefManager.getBoolean(OnboardingFragment.ONBOARDING_PREF, false))
    }
}
