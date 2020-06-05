/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.screenshots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.rule.ActivityTestRule
import tools.fastlane.screengrab.Screengrab
import tools.fastlane.screengrab.locale.LocaleTestRule
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.bookmarksMenu
import org.mozilla.fenix.ui.robots.mDevice
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.swipeToBottom

class MenuScreenShotTest : ScreenshotTest() {
    private lateinit var mockWebServer: MockWebServer
    @Rule
    @JvmField
    val localeTestRule = LocaleTestRule()

    @get:Rule
    var mActivityTestRule: ActivityTestRule<HomeActivity> = HomeActivityTestRule()

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            setDispatcher(AndroidAssetDispatcher())
            start()
        }
    }

    @After
    fun tearDown() {
        mActivityTestRule.getActivity().finishAndRemoveTask()
        mockWebServer.shutdown()
    }

    @Test
    fun threeDotMenuTest() {
        homeScreen {
        }.openThreeDotMenu {
            Screengrab.screenshot("ThreeDotMenuMainRobot_three-dot-menu")
        }
    }

    @Test
    fun settingsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            Screengrab.screenshot("SettingsRobot_settings-menu")
            settingsAccountPreferences()
            Screengrab.screenshot("AccountSettingsRobot_settings-account")
            mDevice.pressBack()

            settingsSearch()
            Screengrab.screenshot("SettingsSubMenuSearchRobot_settings-search")
            mDevice.pressBack()

            settingsTheme()
            Screengrab.screenshot("SettingsSubMenuThemeRobot_settings-theme")
            mDevice.pressBack()

            settingsAccessibility()
            Screengrab.screenshot("SettingsSubMenuAccessibilityRobot_settings-accessibility")
            mDevice.pressBack()

            settingsLanguage()
            Screengrab.screenshot("SettingsSubMenuAccessibilityRobot_settings-language")
            mDevice.pressBack()

            settingDefaultBrowser()
            Screengrab.screenshot("SettingsSubMenuDefaultBrowserRobot_settings-default-browser")
            mDevice.pressBack()

            settingsTP()
            Screengrab.screenshot("settings-enhanced-tp")
            mDevice.pressBack()

            loginsAndPassword()
            Screengrab.screenshot("SettingsSubMenuLoginsAndPasswords-settings-logins-passwords")
            mDevice.pressBack()

            swipeToBottom()
            Screengrab.screenshot("SettingsRobot_settings-scroll-to-bottom")

            settingsTelemetry()
            Screengrab.screenshot("settings-telemetry")
            mDevice.pressBack()

            addOns()
            Screengrab.screenshot("settings-addons")
        }
    }

    @Test
    fun historyTest() {
        homeScreen {
        }.openThreeDotMenu {
        }
        openHistoryThreeDotMenu()
        Screengrab.screenshot("HistoryRobot_history-menu")
    }

    @Test
    fun bookmarksManagementTest() {
        homeScreen {
        }.openThreeDotMenu {
        }
        openBookmarksThreeDotMenu()
        Screengrab.screenshot("BookmarksRobot_bookmarks-menu")
        bookmarksMenu {
            clickAddFolderButton()
            Screengrab.screenshot("BookmarksRobot_add-folder-view")
            saveNewFolder()
            Screengrab.screenshot("BookmarksRobot_error-empty-folder-name")
            addNewFolderName("test")
            saveNewFolder()
        }.openThreeDotMenu {
            Screengrab.screenshot("ThreeDotMenuBookmarksRobot_folder-menu")
        }
        editBookmarkFolder()
        Screengrab.screenshot("ThreeDotMenuBookmarksRobot_edit-bookmark-folder-menu")
        // It may be needed to wait here to have the screenshot
        mDevice.pressBack()
        bookmarksMenu {
        }.openThreeDotMenu {
            deleteBookmarkFolder()
            Screengrab.screenshot("ThreeDotMenuBookmarksRobot_delete-bookmark-folder-menu")
        }
    }

    @Test
    fun collectionMenuTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        navigationToolbar {
            Screengrab.screenshot("NavigationToolbarRobot_navigation-toolbar")
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            Screengrab.screenshot("BrowserRobot_enter-url")
        }
        tapOnTabCounter()
        //  Homescreen with visited tabs
        Screengrab.screenshot("HomeScreenRobot_homescreen-with-tabs-open")
        homeScreen {
        }.openTabsListThreeDotMenu {
            Screengrab.screenshot("open-tabs-menu")
        }.close {
            // It may be needed to wait here for tests working on Firebase
            saveToCollectionButton()
            Screengrab.screenshot("HomeScreenRobot_save-collection-view")
            typeCollectionName("CollectionName")
            mDevice.pressBack()
            // It may be needed to wait here for tests working on Firebase
            Screengrab.screenshot("HomeScreenRobot_saved-collection")
        }
    }

    @Test
    fun tabMenuTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
            Screengrab.screenshot("browser-tab-menu")
        }.closeBrowserMenuToBrowser {
        }.openTabDrawer {
            Screengrab.screenshot("tab-drawer-with-tabs")
            closeTab()
            Screengrab.screenshot("remove-tab")
        }
    }

    @Test
    fun saveLoginPromptTest() {
        val saveLoginTest =
                TestAssetHelper.getSaveLoginAsset(mockWebServer)
        TestAssetHelper.waitingTimeShort
        navigationToolbar {
        }.enterURLAndEnterToBrowser(saveLoginTest.url) {
            Screengrab.screenshot("save-login-prompt")
            TestAssetHelper.waitingTime
            // verifySaveLoginPromptIsShown()
        }
    }
}

fun openHistoryThreeDotMenu() = onView(withText(R.string.library_history)).click()

fun openBookmarksThreeDotMenu() = onView(withText(R.string.library_bookmarks)).click()

fun editBookmarkFolder() = onView(withText(R.string.bookmark_menu_edit_button)).click()

fun deleteBookmarkFolder() = onView(withText(R.string.bookmark_menu_delete_button)).click()

fun saveToCollectionButton() = onView(withId(R.id.save_tab_group_button)).click()

fun tapOnTabCounter() = onView(withId(R.id.counter_text)).click()

fun settingsAccountPreferences() = onView(withText(R.string.preferences_sync)).click()

fun settingsSearch() = onView(withText(R.string.preferences_search)).click()

fun settingsTheme() = onView(withText(R.string.preferences_customize)).click()

fun settingsAccessibility() = onView(withText(R.string.preferences_accessibility)).click()

fun settingDefaultBrowser() = onView(withText(R.string.preferences_set_as_default_browser)).click()

fun settingsToolbar() = onView(withText(R.string.preferences_toolbar)).click()

fun settingsTP() = onView(withText(R.string.preference_enhanced_tracking_protection)).click()

fun settingsAddToHomeScreen() = onView(withText(R.string.preferences_add_private_browsing_shortcut)).click()

fun settingsRemoveData() = onView(withText(R.string.preferences_delete_browsing_data)).click()

fun settingsTelemetry() = onView(withText(R.string.preferences_data_collection)).click()

fun loginsAndPassword() = onView(withText(R.string.preferences_passwords_logins_and_passwords)).click()

fun addOns() = onView(withText(R.string.preferences_addons)).click()

fun settingsLanguage() = onView(withText(R.string.preferences_language)).click()
