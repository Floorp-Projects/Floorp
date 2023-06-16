/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiSelector
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.RetryTestRule
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.ui.robots.clickRateButtonGooglePlay
import org.mozilla.fenix.ui.robots.homeScreen

/**
 *  Tests for verifying the main three dot menu options
 *
 */

class SettingsAboutTest {
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityIntentTestRule = HomeActivityIntentTestRule()

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setUp() {
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    // Walks through settings menu and sub-menus to ensure all items are present
    @Test
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
        activityIntentTestRule.applySettingsExceptions {
            it.isTCPCFREnabled = false
        }

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
        activityIntentTestRule.applySettingsExceptions {
            it.isJumpBackInCFREnabled = false
            it.isTCPCFREnabled = false
        }
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
