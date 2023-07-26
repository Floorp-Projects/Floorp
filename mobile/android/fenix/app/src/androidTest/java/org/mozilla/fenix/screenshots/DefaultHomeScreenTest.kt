/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.fenix.screenshots

import android.os.SystemClock
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import androidx.test.uiautomator.UiDevice
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.ui.robots.homeScreen
import tools.fastlane.screengrab.Screengrab
import tools.fastlane.screengrab.locale.LocaleTestRule

class DefaultHomeScreenTest : ScreenshotTest() {
    private lateinit var mDevice: UiDevice

    @Rule @JvmField
    val localeTestRule = LocaleTestRule()

    @get:Rule
    var mActivityTestRule: ActivityTestRule<HomeActivity> = HomeActivityTestRule()

    @Before
    fun setUp() {
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    }

    @After
    fun tearDown() {
        mActivityTestRule.getActivity().finishAndRemoveTask()
    }

    @Test
    fun showDefaultHomeScreen() {
        homeScreen {
            verifyHomeScreen()
            Screengrab.screenshot("HomeScreenRobot_home-screen-scroll")
            TestAssetHelper.waitingTime
        }
    }

    @Test
    fun privateBrowsingTest() {
        homeScreen {
            SystemClock.sleep(TestAssetHelper.waitingTimeShort)
            Screengrab.screenshot("HomeScreenRobot_home-screen")
        }.openThreeDotMenu {
        }.openSettings {
        }.openPrivateBrowsingSubMenu {
            clickPrivateModeScreenshotsSwitch()
        }
        // To get private screenshot,
        // dismiss onboarding going to settings and back
        mDevice.pressBack()
        mDevice.pressBack()
        homeScreen {
            togglePrivateBrowsingModeOnOff()
            Screengrab.screenshot("HomeScreenRobot_private-browsing-menu")
            togglePrivateBrowsingModeOnOff()
            Screengrab.screenshot("HomeScreenRobot_after-onboarding")
        }
    }
}
