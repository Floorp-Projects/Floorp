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
import org.mozilla.fenix.helpers.FeatureSettingsHelper
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
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer

    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.
    @get:Rule
    val activityTestRule = HomeActivityTestRule()
    private val featureSettingsHelper = FeatureSettingsHelper()

    @Before
    fun setUp() {
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
        featureSettingsHelper.setJumpBackCFREnabled(false)
        featureSettingsHelper.setTCPCFREnabled(false)
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @Test
    fun goBackTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val nextWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(nextWebPage.url) {
            verifyUrl(nextWebPage.url.toString())
        }.openThreeDotMenu {
        }.goBack {
            mDevice.waitForIdle()
            verifyUrl(defaultWebPage.url.toString())
        }
    }

    @Test
    fun goForwardTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val nextWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(nextWebPage.url) {
            mDevice.waitForIdle()
            verifyUrl(nextWebPage.url.toString())
        }.openThreeDotMenu {
        }.goBack {
            mDevice.waitForIdle()
            verifyUrl(defaultWebPage.url.toString())
        }

        // Re-open the three-dot menu for verification
        navigationToolbar {
        }.openThreeDotMenu {
            verifyThreeDotMenuExists()
        }.goForward {
            verifyUrl(nextWebPage.url.toString())
        }
    }

    @Test
    // Test running on beta/release builds in CI:
    // caution when making changes to it, so they don't block the builds
    fun visitURLTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            verifyUrl(defaultWebPage.url.toString())
        }
    }

    @Test
    fun findInPageTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 3)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
            verifyThreeDotMenuExists()
            verifyFindInPageButton()
        }.openFindInPage {
            verifyFindInPageNextButton()
            verifyFindInPagePrevButton()
            verifyFindInPageCloseButton()
            enterFindInPageQuery("a")
            verifyFindNextInPageResult("1/3")
            clickFindInPageNextButton()
            verifyFindNextInPageResult("2/3")
            clickFindInPageNextButton()
            verifyFindNextInPageResult("3/3")
            clickFindInPagePrevButton()
            verifyFindPrevInPageResult("2/3")
            clickFindInPagePrevButton()
            verifyFindPrevInPageResult("1/3")
            enterFindInPageQuery("3")
            verifyFindNextInPageResult("1/1")
        }.closeFindInPage { }
    }
}
