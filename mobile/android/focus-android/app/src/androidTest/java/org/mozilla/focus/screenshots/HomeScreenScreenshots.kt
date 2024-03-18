/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
@file:Suppress("DEPRECATION")

package org.mozilla.focus.screenshots

import android.os.SystemClock
import androidx.appcompat.app.AppCompatDelegate
import androidx.test.espresso.Espresso
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.rule.ActivityTestRule
import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiSelector
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.focus.R
import org.mozilla.focus.activity.MainActivity
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.waitForWebContent
import tools.fastlane.screengrab.Screengrab
import tools.fastlane.screengrab.locale.LocaleTestRule
import java.util.Locale

class HomeScreenScreenshots : ScreenshotTest() {
    @Rule @JvmField
    var mActivityTestRule: ActivityTestRule<MainActivity> =
        object : MainActivityFirstrunTestRule(true, false) {
        }

    @Rule @JvmField
    val localeTestRule = LocaleTestRule()

    @Before
    fun setUp() {
        mActivityTestRule.runOnUiThread {
            AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES)
        }
    }

    @Test
    fun takeScreenshotsOfHomeScreen() {
        onView(withId(R.id.mozac_browser_toolbar_edit_url_view))
            .check(matches(isDisplayed()))
            .check(matches(ViewMatchers.hasFocus()))
        SystemClock.sleep(5000)
        editURLBar.click()
        typeInSearchBar("")

        Screengrab.screenshot("Home_View")
    }

    @Test
    fun takeScreenshotShortCutsHomeScreen() {
        homeScreen {
            addSiteToShortCuts("mozilla.com")
            addSiteToShortCuts("pocket.com")
            addSiteToShortCuts("relay.com")
            addSiteToShortCuts("monitor.firefox.com")
            TestHelper.mDevice.waitForIdle()
            Espresso.closeSoftKeyboard()
            SystemClock.sleep(5000)
            Screengrab.screenshot("ShortCuts")
        }
    }

    @Test
    fun openWebsiteFocus() {
        var currentLocale: String = Locale.getDefault().getLanguage()

        editURLBar.click()
        SystemClock.sleep(1000)

        typeInSearchBar("www.mozilla.org/" + currentLocale + "/firefox/browsers/mobile/focus/")
        TestHelper.mDevice.waitForIdle()
        TestHelper.mDevice.pressEnter()
        waitForWebContent()
        TestHelper.waitForWebSiteTitleLoad()

        SystemClock.sleep(30000)
        waitForWebContent()
        Screengrab.screenshot("FocusWebsite")
    }

    private fun addSiteToShortCuts(website: String) {
        editURLBar.click()
        SystemClock.sleep(1000)
        typeInSearchBar(website)
        TestHelper.mDevice.pressEnter()
        TestHelper.mDevice.waitForIdle()
        menuButton.click()
        TestHelper.mDevice.waitForIdle()
        addToShortCuts()
        TestHelper.mDevice.waitForIdle()
        eraseButton.click()
        TestHelper.mDevice.waitForIdle()
    }
    private val editURLBar: UiObject =
        TestHelper.mDevice.findObject(
            UiSelector().resourceId("${TestHelper.packageName}:id/mozac_browser_toolbar_edit_url_view"),
        )

    private fun typeInSearchBar(searchString: String) {
        searchBar.clearTextField()
        searchBar.setText(searchString)
    }

    private val searchBar =
        TestHelper.mDevice.findObject(UiSelector().resourceId("${TestHelper.packageName}:id/mozac_browser_toolbar_edit_url_view"))

    private val menuButton =
        TestHelper.mDevice.findObject(UiSelector().resourceId("${TestHelper.packageName}:id/mozac_browser_toolbar_menu"))

    private fun addToShortCuts(): ViewInteraction? = onView(ViewMatchers.withText(R.string.menu_add_to_shortcuts)).perform(
        ViewActions.click(),
    )

    private val eraseButton =
        TestHelper.mDevice.findObject(UiSelector().resourceId("${TestHelper.packageName}:id/erase"))
}
