/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.Rule
import org.junit.Before
import org.junit.After
import org.junit.Ignore
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule

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

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun settingsMenuBasicsItemsTests() {
        // Open 3dot (main) menu
        // Select settings

        // Verify header: "Basics"

        // Verify item: "Search Engine" and default value: "Google"
        // Open 3dot (main) menu
        // Select settings
        // Verify default search engine (Google)
        // Select "Search engine" to change
        // Verify menu choices: Google, Amazon.com, Bing, DuckDuckGo, Twitter, Wikipedia
        // Verify label: "Show search suggestions"
        // Verify search suggestions toggle, set to 'on' by default
        // Verify label: "Show visited sites and bookmarks"
        // Verify visited sites and bookmarks toggle, set to 'on' by default

        // Verify item: "Theme" and default value: "Light"
        // Verify item: "Accessibility"
        // Verify item: "Set as default browser" and default toggle value: "off"
        // Verify item: "Search Engine" and default value: "Google"
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

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun changeThemeSetting() {
        // Open 3dot (main) menu
        // Select settings
        // Verify default theme appears as "Light"
        // Select theme to enter theme sub-menu
        // Verify them sub-menu has 3 options: "Light", "Dark" and "Set by Battery Saver"
        // Select "Dark" theme
        // Verify them is changed to Dark
        // Optional:
        // Select "Set by battery saver"
        // Verify theme changes based on battery saver
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
