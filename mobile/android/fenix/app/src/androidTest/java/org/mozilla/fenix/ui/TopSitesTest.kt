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
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 * Tests Top Sites functionality
 *
 * - Verifies 'Add to Firefox Home' UI functionality
 * - Verifies 'Top Sites' context menu UI functionality
 * - Verifies 'Top Site' usage UI functionality
 * - Verifies existence of default top sites available on the home-screen
 */

class TopSitesTest {
    private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityIntentTestRule = HomeActivityIntentTestRule()

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    @Test
    fun verifyAddToFirefoxHome() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val defaultWebPageTitle = "Test_Page_1"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
            expandMenu()
            verifyAddToTopSitesButton()
        }.addToFirefoxHome {
            verifySnackBarText("Added to top sites!")
        }.openTabDrawer {
        }.openNewTab {
        }.dismissSearchBar {
            verifyExistingTopSitesList()
            verifyExistingTopSitesTabs(defaultWebPageTitle)
        }
    }

    @Test
    fun verifyOpenTopSiteNormalTab() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val defaultWebPageTitle = "Test_Page_1"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
            expandMenu()
            verifyAddToTopSitesButton()
        }.addToFirefoxHome {
            verifySnackBarText("Added to top sites!")
        }.openTabDrawer {
        }.openNewTab {
        }.dismissSearchBar {
            verifyExistingTopSitesList()
            verifyExistingTopSitesTabs(defaultWebPageTitle)
        }.openTopSiteTabWithTitle(title = defaultWebPageTitle) {
            verifyUrl(defaultWebPage.url.toString().replace("http://", ""))
        }.openTabDrawer {
        }.openNewTab {
        }.dismissSearchBar {
            verifyExistingTopSitesList()
            verifyExistingTopSitesTabs(defaultWebPageTitle)
        }.openContextMenuOnTopSitesWithTitle(defaultWebPageTitle) {
            verifyTopSiteContextMenuItems()
        }

        // Dismiss context menu popup
        mDevice.pressBack()
    }

    @Test
    fun verifyOpenTopSitePrivateTab() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val defaultWebPageTitle = "Test_Page_1"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
            expandMenu()
            verifyAddToTopSitesButton()
        }.addToFirefoxHome {
            verifySnackBarText("Added to top sites!")
        }.openTabDrawer {
        }.openNewTab {
        }.dismissSearchBar {
            verifyExistingTopSitesList()
            verifyExistingTopSitesTabs(defaultWebPageTitle)
        }.openContextMenuOnTopSitesWithTitle(defaultWebPageTitle) {
            verifyTopSiteContextMenuItems()
        }.openTopSiteInPrivateTab {
            verifyCurrentPrivateSession(activityIntentTestRule.activity.applicationContext)
        }
    }

    @Test
    fun verifyRenameTopSite() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val defaultWebPageTitle = "Test_Page_1"
        val defaultWebPageTitleNew = "Test_Page_2"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
            expandMenu()
            verifyAddToTopSitesButton()
        }.addToFirefoxHome {
            verifySnackBarText("Added to top sites!")
        }.openTabDrawer {
        }.openNewTab {
        }.dismissSearchBar {
            verifyExistingTopSitesList()
            verifyExistingTopSitesTabs(defaultWebPageTitle)
        }.openContextMenuOnTopSitesWithTitle(defaultWebPageTitle) {
            verifyTopSiteContextMenuItems()
        }.renameTopSite(defaultWebPageTitleNew) {
            verifyExistingTopSitesList()
            verifyExistingTopSitesTabs(defaultWebPageTitleNew)
        }
    }

    @Test
    fun verifyRemoveTopSite() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val defaultWebPageTitle = "Test_Page_1"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
            expandMenu()
            verifyAddToTopSitesButton()
        }.addToFirefoxHome {
            verifySnackBarText("Added to top sites!")
        }.openTabDrawer {
        }.openNewTab {
        }.dismissSearchBar {
            verifyExistingTopSitesList()
            verifyExistingTopSitesTabs(defaultWebPageTitle)
        }.openContextMenuOnTopSitesWithTitle(defaultWebPageTitle) {
            verifyTopSiteContextMenuItems()
        }.removeTopSite {
            verifyNotExistingTopSitesList(defaultWebPageTitle)
        }
    }

    @Test
    fun verifyDefaultTopSitesLocale_EN() {
        // en-US defaults
        val defaultTopSites = arrayOf(
            "Top Articles",
            "Wikipedia",
            "Google"
        )

        homeScreen { }.dismissOnboarding()

        homeScreen {
            verifyExistingTopSitesList()
            defaultTopSites.forEach { item ->
                verifyExistingTopSitesTabs(item)
            }
        }
    }
}
