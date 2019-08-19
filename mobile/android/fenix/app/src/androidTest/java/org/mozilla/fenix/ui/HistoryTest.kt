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
 *  Tests for verifying basic functionality of history
 *
 */

class HistoryTest {
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
    fun noHistoryItemsInCacheTest() {
        homeScreen { }.dismissOnboarding()

        // Verify "Your Library" in 3-dot menu is visible
        // Verify "History" line-item in library is visible
        // Verify "No history here" is visible
        // Verify "History" UI elements
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun historyTest() {
        // Setup:
        //  - Visit a URL
        //  - Visit a second URL
        // Verify browser view exists for each visit
        // Verify "Your Library" in 3-dot menu is visible
        // Click "Your Library"
        // Verify "History" line-item in Library is visible
        // Click "History"
        // Verify "History" UI elements (view is visible)
        // Verify history is added, URLs match history added in Library

        // Verify history 3-dot menu functions:
        // 1. Delete

        // Verify history visibility in new URL search

        // Verify "Delete history"
        // Verify "This will delete all your browsing data."
        // Verify "No history here" UI element

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
