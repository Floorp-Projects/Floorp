/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.content.res.Configuration
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestAssetHelper.getLoremIpsumAsset
import org.mozilla.fenix.ui.robots.checkTextSizeOnWebsite
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying the main three dot menu options
 *
 */

class SettingsBasicsTest {
    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.

    private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityIntentTestRule = HomeActivityIntentTestRule()

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    private fun getUiTheme(): Boolean {
        val mode =
            activityIntentTestRule.activity.resources?.configuration?.uiMode?.and(Configuration.UI_MODE_NIGHT_MASK)

        return when (mode) {
            Configuration.UI_MODE_NIGHT_YES -> true // dark theme is set
            Configuration.UI_MODE_NIGHT_NO -> false // dark theme is not set, using light theme
            else -> false // default option is light theme
        }
    }

    @Test
    // Walks through settings menu and sub-menus to ensure all items are present
    fun settingsMenuBasicsItemsTests() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            verifyBasicsHeading()
            verifySearchEngineButton()
            verifyDefaultBrowserItem()
            verifyTabsItem()
            // drill down to submenu
        }.openSearchSubMenu {
            verifyDefaultSearchEngineHeader()
            verifySearchEngineList()
            verifyShowSearchSuggestions()
            verifyShowSearchShortcuts()

            verifyShowClipboardSuggestions()
            verifySearchBrowsingHistory()
            verifySearchBookmarks()
        }.goBack {
        }.openCustomizeSubMenu {
            verifyThemes()
        }.goBack {
        }.openAccessibilitySubMenu {
            verifyAutomaticFontSizingMenuItems()
        }.goBack {
            // drill down to submenu
        }
    }

    @Test
    fun selectNewDefaultSearchEngine() {
        // Goes through the settings and changes the default search engine, then verifies it has changed.
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            changeDefaultSearchEngine("DuckDuckGo")
        }.goBack {
        }.goBack {
            verifyDefaultSearchEngine("DuckDuckGo")
        }
    }

    @Test
    fun toggleShowVisitedSitesAndBookmarks() {
        // Bookmarks a few websites, toggles the history and bookmarks setting to off, then verifies if the visited and bookmarked websites do not show in the suggestions.
        val page1 = getGenericAsset(mockWebServer, 1)
        val page2 = getGenericAsset(mockWebServer, 2)
        val page3 = getGenericAsset(mockWebServer, 3)

        homeScreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(page1.url) {
        }.openThreeDotMenu {
        }.bookmarkPage { }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(page2.url) {
            verifyUrl(page2.url.toString())
        }.openThreeDotMenu {
        }.bookmarkPage { }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(page3.url) {
            mDevice.waitForIdle()
        }

        navigationToolbar {
            verifyNoHistoryBookmarks()
        }
    }

    @Ignore("Failing, see: https://github.com/mozilla-mobile/fenix/issues/19016")
    @Test
    fun changeThemeSetting() {
        // Goes through the settings and changes the default search engine, then verifies it changes.
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openCustomizeSubMenu {
            verifyThemes()
            selectDarkMode()
            verifyDarkThemeApplied(getUiTheme())
            selectLightMode()
            verifyLightThemeApplied(getUiTheme())
        }
    }

    @Test
    fun changeCloseTabsSetting() {
        // Goes through the settings and verified the close tabs setting options.
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openTabsSubMenu {
            verifyOptions()
        }
    }

    @Test
    fun changeAccessibiltySettings() {
        // Goes through the settings and changes the default text on a webpage, then verifies if the text has changed.
        val fenixApp = activityIntentTestRule.activity.applicationContext as FenixApplication
        val webpage = getLoremIpsumAsset(mockWebServer).url

        // This value will represent the text size percentage the webpage will scale to. The default value is 100%.
        val textSizePercentage = 180

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAccessibilitySubMenu {
            clickFontSizingSwitch()
            verifyEnabledMenuItems()
            changeTextSizeSlider(textSizePercentage)
            verifyTextSizePercentage(textSizePercentage)
        }.goBack {
        }.goBack {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(webpage) {
            checkTextSizeOnWebsite(textSizePercentage, fenixApp.components)
        }.openTabDrawer {
        }.openNewTab {
        }.dismissSearchBar {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAccessibilitySubMenu {
            clickFontSizingSwitch()
            verifyMenuItemsAreDisabled()
        }
    }
}
