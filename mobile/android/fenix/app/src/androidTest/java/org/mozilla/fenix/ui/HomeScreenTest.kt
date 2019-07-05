/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.ui.robots.homeScreen

/**
 *  Tests for verifying the presence of home screen elements
 *
 */

class HomeScreenTest {
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
    fun homeScreenItemsTest() {

        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        // temp work-around until FIRST_RUN pref = false
        // is added: https://github.com/mozilla-mobile/fenix/issues/3777
        homeScreen {
            verifyNavigationToolbar()
        }.completeFirstRun(defaultWebPage.url) {
        }

        homeScreen {
            verifyHomeScreen()
            verifyHomePrivateBrowsingButton()
            verifyHomeMenu()
            verifyHomeWordmark()
            verifyOpenTabsHeader()
            verifyAddTabButton()
            verifyNoTabsOpenedHeader()
            verifyNoTabsOpenedText()
            verifyCollectionsHeader()
            verifyNoCollectionsHeader()
            verifyNoCollectionsText()
            verifyHomeToolbar()
            verifyHomeComponent()
        }
    }
}
