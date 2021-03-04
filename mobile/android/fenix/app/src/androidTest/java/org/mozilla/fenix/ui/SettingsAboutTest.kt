/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiSelector
import okhttp3.mockwebserver.MockWebServer
import org.junit.Rule
import org.junit.Before
import org.junit.After
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.ui.robots.clickRateButtonGooglePlay
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.mDevice

/**
 *  Tests for verifying the main three dot menu options
 *
 */

class SettingsAboutTest {
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
    @Test
    fun verifyRateOnGooglePlayRedirect() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            clickRateButtonGooglePlay()
            verifyGooglePlayRedirect()
            // press back to return to the app, or accept ToS if still visible
            mDevice.pressBack()
            dismissGooglePlayToS()
        }
    }

    @Test
    fun verifyAboutFirefoxPreview() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAboutFirefoxPreview {
            verifyAboutFirefoxPreview()
        }
    }
}

private fun dismissGooglePlayToS() {
    if (mDevice.findObject(UiSelector().textContains("Terms of Service")).exists()) {
        mDevice.findObject(UiSelector().textContains("ACCEPT")).click()
    }
}
