/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.MockWebServerHelper

// Tests the First run onboarding screens
@RunWith(AndroidJUnit4ClassRunner::class)
class OldFirstRunTest {
    private lateinit var webServer: MockWebServer
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get: Rule
    val mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = true, showNewOnboarding = false)

    @Before
    fun setUp() {
        webServer = MockWebServer().apply {
            dispatcher = MockWebServerHelper.AndroidAssetDispatcher()
            start()
        }
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)
    }

    @After
    fun tearDown() {
        webServer.shutdown()
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @Test
    fun firstRunOnboardingTest() {
        homeScreen {
            verifyOnboardingFirstSlide()
            clickOnboardingNextButton()
            verifyOnboardingSecondSlide()
            clickOnboardingNextButton()
            verifyOnboardingThirdSlide()
            clickOnboardingNextButton()
            verifyOnboardingLastSlide()
            clickOnboardingFinishButton()
            verifyEmptySearchBar()
        }
    }

    @Test
    fun skipFirstRunOnboardingTest() {
        homeScreen {
            verifyOnboardingFirstSlide()
            clickOnboardingNextButton()
            verifyOnboardingSecondSlide()
            skipFirstRun()
            verifyEmptySearchBar()
        }
    }
}
