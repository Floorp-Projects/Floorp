package org.mozilla.fenix.ui.screenshots

import android.os.SystemClock
import androidx.test.rule.ActivityTestRule

import org.junit.After
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.HomeActivityTestRule

import tools.fastlane.screengrab.Screengrab
import tools.fastlane.screengrab.locale.LocaleTestRule

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.swipeUp
import androidx.test.espresso.matcher.ViewMatchers.withId

import br.com.concretesolutions.kappuccino.actions.ClickActions
import org.hamcrest.Matchers.allOf
import org.mozilla.fenix.ui.robots.homeScreen

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
        onView(allOf(withId(R.id.menuButton))).perform(click())
        Screengrab.screenshot("three-dot-menu")
        device.pressBack()
    }

    @Test
    fun settingsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }
        settingsButton2()
        Screengrab.screenshot("settings")

        SystemClock.sleep(1000)
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
        }
        settingsButton2()
        onView(withId(R.id.recycler_view)).perform(swipeUp())
        Screengrab.screenshot("settings-scroll-to-bottom")
        SystemClock.sleep(1000)

        settingsRemoveData()
        Screengrab.screenshot("settings-delete-browsing-data")
        device.pressBack()
        SystemClock.sleep(1000)

        settingsTelemetry()
        Screengrab.screenshot("settings-telemetry")
        device.pressBack()
    }

    @Test
    fun libraryTest() {
        homeScreen {
        }.openThreeDotMenu {
        }
        libraryButton()
        Screengrab.screenshot("library")
        bookmarksButton()
        Screengrab.screenshot("library-bookmarks")
        device.pressBack()
        historyButton()
        Screengrab.screenshot("library-history")
    }
}

fun settingsButton2() = ClickActions.click { text(R.string.settings) }
fun libraryButton() = ClickActions.click { text(R.string.browser_menu_your_library) }
fun bookmarksButton() = ClickActions.click { text(R.string.library_bookmarks) }
fun historyButton() = ClickActions.click { text(R.string.library_history) }
fun settingsAccount() = ClickActions.click { text(R.string.preferences_sync) }

fun settingsSearch() = ClickActions.click { text(R.string.preferences_search_engine) }
fun settingsTheme() = ClickActions.click { text(R.string.preferences_theme) }
fun settingsAccessibility() = ClickActions.click { text(R.string.preferences_accessibility) }
fun settingsTp() = ClickActions.click { text(R.string.preferences_tracking_protection) }
fun settingsRemoveData() = ClickActions.click { text(R.string.preferences_delete_browsing_data) }
fun settingsTelemetry() = ClickActions.click { text(R.string.preferences_data_choices) }
