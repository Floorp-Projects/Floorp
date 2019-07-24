package org.mozilla.fenix.ui.screenshots

import android.os.SystemClock
import androidx.test.rule.ActivityTestRule

import org.junit.After
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper

import tools.fastlane.screengrab.Screengrab
import tools.fastlane.screengrab.locale.LocaleTestRule

import br.com.concretesolutions.kappuccino.actions.ClickActions
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.swipeToBottom

class ThreeDotMenuScreenShotTest : ScreenshotTest() {
    @Rule @JvmField
    val localeTestRule = LocaleTestRule()

    @get:Rule
    var mActivityTestRule: ActivityTestRule<HomeActivity> = HomeActivityTestRule()

    @After
    fun tearDown() {
        mActivityTestRule.getActivity().finishAndRemoveTask()
    }

    @Test
    fun threeDotMenu() {
        homeScreen {
        }.openThreeDotMenu { }
        Screengrab.screenshot("three-dot-menu")
        device.pressBack()
    }

    @Test
    fun settingsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings { }
        Screengrab.screenshot("settings")

        SystemClock.sleep(TestAssetHelper.waitingTime)
        settingsAccount()
        Screengrab.screenshot("settings-sync")
        device.pressBack()

        settingsTheme()
        Screengrab.screenshot("settings-theme")
        device.pressBack()

        settingsSearch()
        Screengrab.screenshot("settings-search")
        device.pressBack()

        settingsAccessibility()
        Screengrab.screenshot("settings-accessibility")
        device.pressBack()

        settingsTp()
        Screengrab.screenshot("settings-tp")
        device.pressBack()
    }

    @Test
    fun settingsAfterScrollMenusTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            swipeToBottom()
        }
        Screengrab.screenshot("settings-scroll-to-bottom")
        SystemClock.sleep(TestAssetHelper.waitingTime)

        settingsRemoveData()
        Screengrab.screenshot("settings-delete-browsing-data")
        device.pressBack()
        SystemClock.sleep(TestAssetHelper.waitingTime)

        settingsTelemetry()
        Screengrab.screenshot("settings-telemetry")
        device.pressBack()
    }

    @Test
    fun libraryTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openLibrary { }
        Screengrab.screenshot("library")

        bookmarksButton()
        Screengrab.screenshot("library-bookmarks")
        device.pressBack()
        historyButton()
        Screengrab.screenshot("library-history")
    }
}

fun bookmarksButton() = ClickActions.click { text(R.string.library_bookmarks) }
fun historyButton() = ClickActions.click { text(R.string.library_history) }
fun settingsAccount() = ClickActions.click { text(R.string.preferences_sync) }

fun settingsSearch() = ClickActions.click { text(R.string.preferences_search_engine) }
fun settingsTheme() = ClickActions.click { text(R.string.preferences_theme) }
fun settingsAccessibility() = ClickActions.click { text(R.string.preferences_accessibility) }
fun settingsTp() = ClickActions.click { text(R.string.preferences_tracking_protection) }
fun settingsRemoveData() = ClickActions.click { text(R.string.preferences_delete_browsing_data) }
fun settingsTelemetry() = ClickActions.click { text(R.string.preferences_data_collection) }
