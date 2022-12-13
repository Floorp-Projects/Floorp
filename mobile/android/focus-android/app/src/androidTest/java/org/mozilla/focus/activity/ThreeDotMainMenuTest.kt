/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.MockWebServerHelper
import org.mozilla.focus.helpers.RetryTestRule
import org.mozilla.focus.helpers.TestAssetHelper
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.testAnnotations.SmokeTest

/**
 * Verifies main menu items on the homescreen and on a browser page.
 */
class ThreeDotMainMenuTest {
    private lateinit var webServer: MockWebServer
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get: Rule
    val mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    fun startWebServer() {
        webServer = MockWebServer().apply {
            dispatcher = MockWebServerHelper.AndroidAssetDispatcher()
            start()
        }
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)
    }

    @After
    fun tearDown() {
        webServer.shutdown()
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @SmokeTest
    @Test
    fun homeScreenMenuItemsTest() {
        homeScreen {
        }.openMainMenu {
            verifyHelpPageLinkExists()
            verifySettingsButtonExists()
        }
    }

    @SmokeTest
    @Test
    fun browserMenuItemsTest() {
        val pageUrl = TestAssetHelper.getGenericTabAsset(webServer, 1).url

        searchScreen {
        }.loadPage(pageUrl) {
            verifyPageContent("Tab 1")
        }.openMainMenu {
            verifyShareButtonExists()
            verifyAddToHomeButtonExists()
            verifyFindInPageExists()
            verifyOpenInButtonExists()
            verifyRequestDesktopSiteExists()
            verifySettingsButtonExists()
            verifyReportSiteIssueButtonExists()
        }
    }

    @SmokeTest
    @Test
    fun shareTabTest() {
        val pageUrl = TestAssetHelper.getGenericTabAsset(webServer, 1).url

        searchScreen {
        }.loadPage(pageUrl) {
            progressBar.waitUntilGone(waitingTime)
        }.openMainMenu {
        }.openShareScreen {
            verifyShareAppsListOpened()
            mDevice.pressBack()
        }
    }

    @SmokeTest
    @Test
    fun findInPageTest() {
        val pageUrl = TestAssetHelper.getGenericTabAsset(webServer, 1).url

        searchScreen {
        }.loadPage(pageUrl) {
            progressBar.waitUntilGone(waitingTime)
        }.openMainMenu {
        }.openFindInPage {
            enterFindInPageQuery("tab")
            verifyFindNextInPageResult("1/3")
            verifyFindNextInPageResult("2/3")
            verifyFindNextInPageResult("3/3")
            verifyFindPrevInPageResult("1/3")
            verifyFindPrevInPageResult("3/3")
            verifyFindPrevInPageResult("2/3")
            closeFindInPage()
        }
    }

    @SmokeTest
    @Test
    fun switchDesktopModeTest() {
        val pageUrl = TestAssetHelper.getGenericTabAsset(webServer, 1).url

        searchScreen {
        }.loadPage(pageUrl) {
            progressBar.waitUntilGone(waitingTime)
            verifyPageContent("mobile-site")
        }.openMainMenu {
        }.switchDesktopSiteMode {
            progressBar.waitUntilGone(waitingTime)
            verifyPageContent("desktop-site")
        }.openMainMenu {
            verifyRequestDesktopSiteIsEnabled(true)
        }.switchDesktopSiteMode {
            verifyPageContent("mobile-site")
        }.openMainMenu {
            verifyRequestDesktopSiteIsEnabled(false)
        }
    }
}
