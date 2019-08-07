/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.Before
import org.junit.After
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying basic functionality of tabs
 *
 */

class TabsTest {
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
    fun tabsItemsTest() {
        homeScreen { }.dismissOnboarding()

        // Setup browser so that tabs are visible in UI
        // Verify all tabs elements are visible:
        // "open tabs header, + button, etc.
        // Verify tabs 3-dot menu elements
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun noTabsInCacheTest() {
        // Verify open tabs header and text exists (when no previous browsing)
        // Verify + button redirects to navigation bar UI
        // Verify "Collections" header exists
        // Verify "No collections" text (when no previous browsing)
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun browsingWithTabsTest() {
        // Setup:
        //  - Verify + button redirects to navigation bar UI
        //  - Enter mock website via navigation bar
        // Verify "Open tabs" header exits
        // Verify Collections header exits
        // Verify that tabs counter is augmented by 1 count
        // Click on tabs counter
        // Verify that new page is listed in "Open tabs"
        // Repeat for several sites

    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun tabsThreeDotMenuTest() {
        // short 3-dot menu setup:
        // - create multiple tabs (using mock web server) for the following...
        // Verify tabs 3-dot menu functions:
        //  1. "Close all tabs"
        //  2. "Share tabs" - opens share sub-menu
        //  3. "Save to collection" - verify saved to collection

        // NOTE: extended 3 dot menu test is verified in a separate class
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun collectionsTest() {
        // Setup:
        // - create multiple tabs (using mock web server) for the following...
        // Verify collections header exits
        // Verify multiple collections can be saved, named
        // Verify "Select tabs to save"
        // Verify collections dropdown toggle
        // Verify send and share button works - opens share menu

        // Verify collections 3-dot menu functions:
        // 1. Delete collection
        // 2. Rename collection
        // 3. Open tabs
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
