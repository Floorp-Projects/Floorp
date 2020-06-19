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
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying basic functionality of browser navigation and page related interactions
 *
 *  Including:
 *  - Visiting a URL
 *  - Back and Forward navigation
 *  - Refresh
 *  - Find in page
 */

class NavigationToolbarTest {
    private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    private lateinit var mockWebServer: MockWebServer

    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.
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
    fun goBackTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val nextWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            // verifyPageContent(defaultWebPage.content)
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(nextWebPage.url) {
            // verifyPageContent(nextWebPage.content)
        }

        // Re-open the three-dot menu for verification
        navigationToolbar {
        }.openThreeDotMenu {
            verifyThreeDotMenuExists()
        }.goBack {
            // verifyPageContent(defaultWebPage.content)
        }
    }

    @Test
    fun goForwardTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val nextWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            // verifyPageContent(defaultWebPage.content)
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(nextWebPage.url) {
            // verifyPageContent(nextWebPage.content)
            mDevice.pressBack()
            // verifyPageContent(defaultWebPage.content)
        }

        // Re-open the three-dot menu for verification
        navigationToolbar {
        }.openThreeDotMenu {
            verifyThreeDotMenuExists()
            verifyForwardButton()
        }.goForward {
            // verifyPageContent(nextWebPage.content)
        }
    }

    @Test
    fun refreshPageTest() {
        val refreshWebPage = TestAssetHelper.getRefreshAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(refreshWebPage.url) {
            // verifyPageContent("DEFAULT")
        }

        // Use refresh from the three-dot menu
        navigationToolbar {
        }.openThreeDotMenu {
            verifyThreeDotMenuExists()
            verifyRefreshButton()
        }.refreshPage {
            // verifyPageContent("REFRESHED")
        }
    }

    @Test
    fun visitURLTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            // verifyPageContent(defaultWebPage.content)
        }
    }

    @Ignore("Temp disable broken test - see:  https://github.com/mozilla-mobile/fenix/issues/5534")
    @Test
    fun findInPageTest() {
        val loremIpsumWebPage = TestAssetHelper.getLoremIpsumAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loremIpsumWebPage.url) {
            // verifyPageContent(loremIpsumWebPage.content)
        }

        navigationToolbar {
        }.openThreeDotMenu {
            verifyThreeDotMenuExists()
            verifyFindInPageButton()
        }.openFindInPage {
            verifyFindInPageNextButton()
            verifyFindInPagePrevButton()
            verifyFindInPageCloseButton()
            enterFindInPageQuery("lab")
            verifyFindNextInPageResult("1/3")
            verifyFindNextInPageResult("2/3")
            verifyFindNextInPageResult("3/3")
            verifyFindPrevInPageResult("1/3")
            verifyFindPrevInPageResult("3/3")
            verifyFindPrevInPageResult("2/3")
            enterFindInPageQuery("in")
            verifyFindNextInPageResult("3/7")
            verifyFindNextInPageResult("4/7")
            verifyFindNextInPageResult("5/7")
            verifyFindNextInPageResult("6/7")
            verifyFindNextInPageResult("7/7")
        }.closeFindInPage { }
    }
}
