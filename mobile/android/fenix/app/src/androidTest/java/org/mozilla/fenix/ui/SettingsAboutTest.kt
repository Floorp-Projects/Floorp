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
import org.mozilla.fenix.ui.robots.homeScreen

/**
 *  Tests for verifying the main three dot menu options
 *
 */

class SettingsAboutTest {
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

    @Test
    // Walks through settings menu and sub-menus to ensure all items are present
    fun settingsAboutItemsTest() {
        // ABOUT
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            // ABOUT
            verifyAboutHeading()
            verifyRateOnGooglePlay()
            verifyAboutFirefoxPreview()
        }
    }

    // ABOUT
    @Ignore("This is a stub test, ignore for now")
    @Test
    fun verifyHelpRedirect() {
        // Open 3dot (main) menu
        // Select settings
        // Click on "Help"
        // Verify redirect to: https://support.mozilla.org/
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun verifyRateOnGooglePlayRedirect() {
        // Open 3dot (main) menu
        // Select settings
        // Click on "Rate on Google Play"
        // Verify Android "Open with Google Play Store" sub menu
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun verifyAboutFirefoxPreview() {
        // Open 3dot (main) menu
        // Select settings
        // Click on "Verify About Firefox Preview"
        // Verify about page contains....
        // Build #
        // Version #
        // "Firefox Preview is produced by Mozilla"
        // Day, Date, timestamp
        // "Open source libraries we use"
    }
}
