/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.activity.robots.browserScreen
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.RetryTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.testAnnotations.SmokeTest

// These tests check the interaction with various context menu options
@RunWith(AndroidJUnit4ClassRunner::class)
class ContextMenusTest {
    private lateinit var webServer: MockWebServer

    private val featureSettingsHelper = FeatureSettingsHelper()

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setup() {
        featureSettingsHelper.setShieldIconCFREnabled(false)
        featureSettingsHelper.setNumberOfTabsOpened(4)
        webServer = MockWebServer()
        webServer.enqueue(TestHelper.createMockResponseFromAsset("tab1.html"))
        webServer.enqueue(TestHelper.createMockResponseFromAsset("tab2.html"))
    }

    @After
    fun tearDown() {
        webServer.shutdown()
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @SmokeTest
    @Test
    fun copyLinkAddressTest() {
        val tab1Url = webServer.url("tab1.html").toString()
        val tab2Url = webServer.url("tab2.html").toString()

        searchScreen {
        }.loadPage(tab1Url) {
            verifyPageContent("Tab 1")
            longPressLink("Tab 2")
            verifyLinkContextMenu(tab2Url)
            clickContextMenuCopyLink()
        }

        searchScreen {
            clickToolbar()
            clearSearchBar()
            longPressSearchBar()
            pasteAndLoadLink()
        }

        browserScreen {
            progressBar.waitUntilGone(TestHelper.waitingTime)
            verifyPageURL(tab2Url)
        }
    }
}
