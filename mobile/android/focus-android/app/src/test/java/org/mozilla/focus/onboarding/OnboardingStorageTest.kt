/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.onboarding

import androidx.preference.PreferenceManager
import androidx.test.core.app.ApplicationProvider
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mozilla.focus.R
import org.mozilla.focus.fragment.onboarding.OnboardingStep
import org.mozilla.focus.fragment.onboarding.OnboardingStorage
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class OnboardingStorageTest {
    private lateinit var onBoardingStorage: OnboardingStorage

    @Before
    fun setup() {
        onBoardingStorage = spy(OnboardingStorage(testContext))
    }

    @Test
    fun `GIVEN at the first onboarding step WHEN querying the current onboarding step from storage THEN get the first step`() {
        doReturn(testContext.getString(R.string.pref_key_first_screen))
            .`when`(onBoardingStorage).getCurrentOnboardingStepFromSharedPref()

        assertEquals(
            OnboardingStep.ON_BOARDING_FIRST_SCREEN,
            onBoardingStorage.getCurrentOnboardingStep(),
        )
    }

    @Test
    fun `GIVEN at the second onboarding step WHEN querying the current onboarding step from storage THEN get the second step`() {
        doReturn(testContext.getString(R.string.pref_key_second_screen))
            .`when`(onBoardingStorage).getCurrentOnboardingStepFromSharedPref()

        assertEquals(
            OnboardingStep.ON_BOARDING_SECOND_SCREEN,
            onBoardingStorage.getCurrentOnboardingStep(),
        )
    }

    @Test
    fun `GIVEN onboarding not started WHEN querying the current onboarding step from storage THEN get the first step`() {
        assertEquals(
            OnboardingStep.ON_BOARDING_FIRST_SCREEN,
            onBoardingStorage.getCurrentOnboardingStep(),
        )
    }

    @Test
    fun `GIVEN saveCurrentOnBoardingStepInSharePref is called WHEN ONBOARDING_FIRST_SCREEN is saved, THEN value for pref_key_onboarding_step is pref_key_first_screen`() {
        onBoardingStorage.saveCurrentOnboardingStepInSharePref(OnboardingStep.ON_BOARDING_FIRST_SCREEN)

        val prefManager =
            PreferenceManager.getDefaultSharedPreferences(ApplicationProvider.getApplicationContext())

        assertEquals(testContext.getString(OnboardingStep.ON_BOARDING_FIRST_SCREEN.prefId), prefManager.getString(testContext.getString(R.string.pref_key_onboarding_step), ""))
    }

    @Test
    fun `GIVEN saveCurrentOnBoardingStepInSharePref is called WHEN ONBOARDING_SECOND_SCREEN is saved, THEN value for pref_key_onboarding_step is pref_key_second_screen`() {
        onBoardingStorage.saveCurrentOnboardingStepInSharePref(OnboardingStep.ON_BOARDING_SECOND_SCREEN)

        val prefManager =
            PreferenceManager.getDefaultSharedPreferences(ApplicationProvider.getApplicationContext())

        assertEquals(testContext.getString(OnboardingStep.ON_BOARDING_SECOND_SCREEN.prefId), prefManager.getString(testContext.getString(R.string.pref_key_onboarding_step), ""))
    }
}
