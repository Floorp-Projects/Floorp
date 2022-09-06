/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.onboarding

import android.os.Build
import androidx.preference.PreferenceManager
import androidx.test.core.app.ApplicationProvider
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations
import org.mozilla.focus.R
import org.mozilla.focus.fragment.onboarding.DefaultOnboardingController
import org.mozilla.focus.fragment.onboarding.OnboardingController
import org.mozilla.focus.fragment.onboarding.OnboardingStorage
import org.mozilla.focus.state.AppStore
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
class OnboardingControllerTest {

    @Mock
    private lateinit var appStore: AppStore

    @Mock
    private lateinit var onboardingStorage: OnboardingStorage
    private lateinit var onboardingController: OnboardingController

    @Before
    fun init() {
        MockitoAnnotations.openMocks(this)
        onboardingController = spy(
            DefaultOnboardingController(
                onboardingStorage,
                appStore,
                ApplicationProvider.getApplicationContext(),
                "1",
            ),
        )
    }

    @Test
    fun `GIVEN onBoarding, WHEN start browsing is pressed, THEN onBoarding flag is true`() {
        DefaultOnboardingController(
            onboardingStorage,
            appStore,
            ApplicationProvider.getApplicationContext(),
            "1",
        ).handleFinishOnBoarding()

        val prefManager =
            PreferenceManager.getDefaultSharedPreferences(ApplicationProvider.getApplicationContext())

        assertEquals(false, prefManager.getBoolean(testContext.getString(R.string.firstrun_shown), false))
    }

    @Config(sdk = [Build.VERSION_CODES.M])
    @Test
    fun `GIVEN onBoarding and build version is M, WHEN get started button is pressed, THEN onBoarding flow must end`() {
        onboardingController.handleGetStartedButtonClicked()

        verify(onboardingController, times(1)).handleFinishOnBoarding()
    }
}
