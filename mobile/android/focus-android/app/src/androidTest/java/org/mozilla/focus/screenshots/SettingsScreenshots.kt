/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
@file:Suppress("DEPRECATION")

package org.mozilla.focus.screenshots

import android.os.SystemClock
import androidx.appcompat.app.AppCompatDelegate
import androidx.appcompat.app.AppCompatDelegate.MODE_NIGHT_YES
import androidx.test.rule.ActivityTestRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.focus.activity.MainActivity
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper
import tools.fastlane.screengrab.Screengrab
import tools.fastlane.screengrab.locale.LocaleTestRule

class SettingsScreenshots : ScreenshotTest() {
    @Rule
    @JvmField
    var mActivityTestRule: ActivityTestRule<MainActivity> =
        object : MainActivityFirstrunTestRule(true, false) {
        }

    @Before
    fun setUp() {
        mActivityTestRule.runOnUiThread {
            AppCompatDelegate.setDefaultNightMode(MODE_NIGHT_YES)
        }
    }

    @Rule
    @JvmField
    val localeTestRule = LocaleTestRule()

    @Test
    fun takeScreenshotOfSiteSettings() {
        val pageUrl = "https://www.mozilla.org"

        searchScreen {
        }.loadPage(pageUrl) {
            verifySiteTrackingProtectionIconShown()
            SystemClock.sleep(5000)
            TestHelper.waitForWebSiteTitleLoad()
            TestHelper.waitForWebContent()
            SystemClock.sleep(3000)
        }.openSiteSettingsMenu {
            SystemClock.sleep(5000)
            Screengrab.screenshot("SiteSettingsSubMenu_View")
        }
    }

    @Test
    fun takeScreenshotOfSearchSettings() {
        homeScreen {
        }.openMainMenu {
        }.openSettings {
            verifySettingsMenuItems()
        }.openSearchSettingsMenu {
            verifySearchSettingsItems()
            openSearchEngineSubMenu()
            SystemClock.sleep(5000)
            Screengrab.screenshot("SearchEngineSubMenu_View")
        }
    }
}
