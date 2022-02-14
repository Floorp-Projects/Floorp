/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
@file:Suppress("DEPRECATION")

package org.mozilla.focus.screenshots

import android.os.SystemClock
import androidx.test.rule.ActivityTestRule
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.focus.activity.MainActivity
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import tools.fastlane.screengrab.Screengrab
import tools.fastlane.screengrab.locale.LocaleTestRule

class FirstRunScreenshots : ScreenshotTest() {
    @Rule
    @JvmField
    var mActivityTestRule: ActivityTestRule<MainActivity> =
        object : MainActivityFirstrunTestRule(true, true) {
        }

    @Rule
    @JvmField
    val localeTestRule = LocaleTestRule()

    @Ignore
    @Test
    fun takeScreenshotsOfFirstrun() {
        homeScreen {
            verifyOnboardingFirstSlide()
            device.waitForIdle()
            SystemClock.sleep(5000)
            Screengrab.screenshot("Onboarding_1_View")

            clickOnboardingNextBtn()

            verifyOnboardingSecondSlide()
            Screengrab.screenshot("Onboarding_2_View")

            clickOnboardingNextBtn()

            verifyOnboardingThirdSlide()
            Screengrab.screenshot("Onboarding_3_View")

            clickOnboardingNextBtn()

            verifyOnboardingLastSlide()
            Screengrab.screenshot("Onboarding_last_View")
        }
    }
}
