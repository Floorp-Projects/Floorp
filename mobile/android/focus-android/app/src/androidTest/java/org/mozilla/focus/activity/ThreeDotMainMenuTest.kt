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
import org.mozilla.focus.helpers.RetryTestRule
import org.mozilla.focus.helpers.TestHelper.createMockResponseFromAsset
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
        webServer = MockWebServer()
        webServer.enqueue(createMockResponseFromAsset("tab1.html"))
        webServer.start()
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)
        featureSettingsHelper.setNumberOfTabsOpened(4)
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
        val pageUrl = webServer.url("tab1.html").toString()

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
        /* Go to a webpage */
        searchScreen {
        }.loadPage(webServer.url("").toString()) {
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
        searchScreen {
        }.loadPage(webServer.url("").toString()) {
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
        searchScreen {
        }.loadPage(webServer.url("").toString()) {
            progressBar.waitUntilGone(waitingTime)
            verifyPageContent("Tab 1")
        }.openMainMenu {
        }.switchDesktopSiteMode {
            progressBar.waitUntilGone(waitingTime)
            verifyPageContent("Tab 1")
        }.openMainMenu {
            verifyRequestDesktopSiteIsEnabled(true)
        }.switchDesktopSiteMode {
        }.openMainMenu {
            verifyRequestDesktopSiteIsEnabled(false)
        }
    }
}
