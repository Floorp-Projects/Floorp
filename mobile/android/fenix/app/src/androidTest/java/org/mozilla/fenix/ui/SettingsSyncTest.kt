/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

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

/**
 *  Tests for verifying the main three dot menu options
 *
 */

class SettingsSyncTest {
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

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

    // Walks through settings sync menu and sub-menus to ensure all items are present
    @Ignore("This is a stub test, ignore for now")
    @Test
    fun settingsSyncItemsTest() {
        // SYNC

        // Open 3dot (main) menu
        // Select settings
        // Verify header: "Turn on Sync"
        // Verify description: "Sync bookmarks, history, and more with your Firefox Account"
    }

    // SYNC
    @Ignore("This is a stub test, ignore for now")
    @Test
    fun turnOnSync() {
        // Note this requires a test Firefox Account and a desktop
        // Open 3dot (main) menu
        // Select settings
        // Click on "Turn on Sync"
        // Open Firefox on laptop and go to https://firefox.com/pair
        // Pair with QR code and/or alternate method
        // Verify pairing
    }
}
