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
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.ui.robots.homeScreen

/**
 *  Tests for verifying the main three dot menu options
 *
 */

class SettingsBasicsTest {
    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.

    private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            setDispatcher(AndroidAssetDispatcher())
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    private fun getUiTheme(): Boolean {
        val mode = activityTestRule.activity.resources?.configuration?.uiMode?.and(Configuration.UI_MODE_NIGHT_MASK)

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
        }.openThemeSubMenu {
            verifyThemes()
        }.goBack {
        }.openAccessibilitySubMenu {
            verifyAutomaticFontSizing()
        }.goBack {
            // drill down to submenu
        }.openDefaultBrowserSubMenu {
            // verify item: set as default browser (duplicates, verify child of recyclerview)
            // Verify label: Open links in private tab
        }.goBack {
        }
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun selectNewDefaultSearchEngine() {
        // Open 3dot (main) menu
        // Select settings
        // Select "Search engine"
        // Choose: DuckDuckGo
        // Back arrow to Home
        // Verify DuckDuckGo icon in Navigation bar
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun toggleSearchSuggestions() {
        // Enter: "mozilla" in navigation bar
        // Verify more than one suggesion provided
        // Open 3dot (main) menu
        // Select settings
        // Select "Search engine"
        // Toggle 'Show search suggestions' to 'off'
        // Back arrow twice to home screen
        // Enter: "mozilla" in navigation bar
        // Verify no suggestions provided
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun toggleShowVisitedSitesAndBookmarks() {
        // Visit 3 static sites
        // Bookmark 2 of them
        // Open 3dot (main) menu
        // Enter navigation bar and verify visited sites appear
        // Verify bookmarks exist
        // Open 3dot (main) menu
        // Select settings
        // Select "Search engine"
        // Toggle off "Show visited sites and bookmarks"
        // Back arrow twice to home screen
        // Verify history and bookmarks are gone
    }

    @Test
    fun changeThemeSetting() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openThemeSubMenu {
            verifyThemes()
            selectDarkMode()
            verifyDarkThemeApplied(getUiTheme())
            selectLightMode()
            verifyLightThemeApplied(getUiTheme())
        }
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun changeAccessibiltySettings() {
        // Open 3dot (main) menu
        // Select settings
        // Select Accessibility
        // Verify header:  "Automatic Font Sizing"
        // Verify description: "Font size will match your Android settings. Disable to manage font size here"
        // Verify toggle is set to 'on' by default
        // Toggle font size to off
        // Verify that new sub-menu items appear....
        // Verify header: "Font Size"
        // Verify description: "Make text on websites larger or smaller"
        // Verify slider bar exists
        // Verify slider bar default value set to 100%
        // Verify sample text "The quick brown fox..." appears 4 times
        // Move slider bar to 180%
        // Verify that text grows to 180%
        // Back error twice to home screen
        // Open static website in navigation bar
        // Verify that text is now at 180%
        // Select settings
        // Select Accessibility
        // Toggle font size back to 'off'
        // Verify that "Font Size" header, description, slider bar and sample text all disappear
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun changeDefaultBrowserSetting() {
        // Open 3dot (main) menu
        // Select settings
        // Verify that "Set as default browser toggle is set to 'off' (default)
        // Turn default browser toggle 'on'
        // Verify that Andrdoid "Default Apps" menu appears
    }
}
