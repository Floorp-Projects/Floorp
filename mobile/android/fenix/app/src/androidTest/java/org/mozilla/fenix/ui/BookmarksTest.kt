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
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying basic functionality of bookmarks
 *
 */

class BookmarksTest {
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
    fun noBookmarkItemsInCacheTest() {
        homeScreen { }.dismissOnboarding()

        // Verify "Your Library" in 3-dot menu is visible
        // Verify "Bookmarks" line-item in library is visible
        // Verify UI elements
        // Verify "No bookmarks here" is visible
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun bookmarkTest() {
        // Setup:
        //  - Visit a URL
        // Verify browser view exists
        // Swipe up slide-up-tray
        // Verify "slide-up tray" is visible
        // Verify "Bookmark" is visible in "slide-up tray"
        // Click "Bookmark"
        // Verify "Bookmark saved" toast notification is visible
        // Verify "Your Library" in 3-dot menu is visible
        // Click "Your Library"
        // Verify "Bookmarks" line-item in Library is visible
        // Click  "Bookmarks"
        // Verify "Bookmarks" UI elements (view is visible)
        // Verify bookmark added, URL matches bookmark added in Library

        // Verify bookmarks 3-dot menu functions:
        // 1. Edit
        // 2. Copy
        // 3. Share
        // 4. Open in new tab
        // 5. Open in private tab
        // 6. Delete

        // Verify bookmark visibility in new URL search

        // Verify return to "Your Library"
    }

    @Ignore("This is a sample test, ignore")
    @Test
    fun sampleTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }
    }
}
