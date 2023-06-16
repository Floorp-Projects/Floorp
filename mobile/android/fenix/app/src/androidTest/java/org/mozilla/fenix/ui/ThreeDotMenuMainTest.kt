/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.clickSnackbarButton
import org.mozilla.fenix.ui.robots.clickContextMenuItem
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.longClickPageObject

/**
 *  Tests for verifying the main three dot menu options
 *
 */

class ThreeDotMenuMainTest {
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityTestRule = HomeActivityTestRule.withDefaultSettingsOverrides()

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

    // Verifies the list of items in the homescreen's 3 dot main menu
    @Test
    fun homeThreeDotMenuItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
            verifyHomeThreeDotMainMenuItems(isRequestDesktopSiteEnabled = false)
        }.openBookmarks {
            verifyBookmarksMenuView()
        }.goBack {
        }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryMenuView()
        }.goBack {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            verifyEmptyDownloadsList()
        }.goBack {
        }.openThreeDotMenu {
        }.openAddonsManagerMenu {
            verifyAddonsItems()
        }.goBack {
        }.openThreeDotMenu {
        }.openSyncSignIn {
            verifyTurnOnSyncMenu()
        }.goBack {
            // Desktop toggle
        }.openThreeDotMenu {
        }.switchDesktopSiteMode {
        }
        homeScreen {
        }.openThreeDotMenu {
            verifyDesktopSiteModeEnabled(isRequestDesktopSiteEnabled = true)
        }.openWhatsNew {
            verifyWhatsNewURL()
        }.goToHomescreen {
        }.openThreeDotMenu {
        }.openHelp {
            verifyHelpUrl()
        }.goToHomescreen {
        }.openThreeDotMenu {
        }.openCustomizeHome {
            verifyHomePageView()
        }.goBackToHomeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            verifySettingsView()
        }
    }

    // Verifies the list of items in the homescreen's 3 dot main menu in private browsing
    @Test
    fun privateHomeThreeDotMenuItemsTest() {
        homeScreen {
        }.togglePrivateBrowsingMode()
        homeScreen {
        }.openThreeDotMenu {
            verifyHomeThreeDotMainMenuItems(isRequestDesktopSiteEnabled = false)
        }.openBookmarks {
            verifyBookmarksMenuView()
        }.goBack {
        }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryMenuView()
        }.goBack {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            verifyEmptyDownloadsList()
        }.goBack {
        }.openThreeDotMenu {
        }.openAddonsManagerMenu {
            verifyAddonsItems()
        }.goBack {
        }.openThreeDotMenu {
        }.openSyncSignIn {
            verifyTurnOnSyncMenu()
        }.goBack {
            // Desktop toggle
        }.openThreeDotMenu {
        }.switchDesktopSiteMode {
        }
        homeScreen {
        }.openThreeDotMenu {
            verifyDesktopSiteModeEnabled(isRequestDesktopSiteEnabled = true)
        }.openWhatsNew {
            verifyWhatsNewURL()
        }.goToHomescreen {
        }.openThreeDotMenu {
        }.openHelp {
            verifyHelpUrl()
        }.goToHomescreen {
        }.openThreeDotMenu {
        }.openCustomizeHome {
            verifyHomePageView()
        }.goBackToHomeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            verifySettingsView()
        }
    }

    @Test
    fun setDesktopSiteBeforePageLoadTest() {
        val webPage = TestAssetHelper.getGenericAsset(mockWebServer, 4)

        homeScreen {
        }.openThreeDotMenu {
            verifyDesktopSiteModeEnabled(false)
        }.switchDesktopSiteMode {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openThreeDotMenu {
            verifyDesktopSiteModeEnabled(true)
        }.closeBrowserMenuToBrowser {
            clickPageObject(itemContainingText("Link 1"))
        }.openThreeDotMenu {
            verifyDesktopSiteModeEnabled(true)
        }.closeBrowserMenuToBrowser {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
            longClickPageObject(itemWithText("Link 2"))
            clickContextMenuItem("Open link in new tab")
            clickSnackbarButton("SWITCH")
        }.openThreeDotMenu {
            verifyDesktopSiteModeEnabled(false)
        }
    }

    @Test
    fun privateBrowsingSetDesktopSiteBeforePageLoadTest() {
        val webPage = TestAssetHelper.getGenericAsset(mockWebServer, 4)

        homeScreen {
        }.togglePrivateBrowsingMode()

        homeScreen {
        }.openThreeDotMenu {
            verifyDesktopSiteModeEnabled(false)
        }.switchDesktopSiteMode {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openThreeDotMenu {
            verifyDesktopSiteModeEnabled(true)
        }.closeBrowserMenuToBrowser {
            clickPageObject(itemContainingText("Link 1"))
        }.openThreeDotMenu {
            verifyDesktopSiteModeEnabled(true)
        }.closeBrowserMenuToBrowser {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
            longClickPageObject(itemWithText("Link 2"))
            clickContextMenuItem("Open link in private tab")
            clickSnackbarButton("SWITCH")
        }.openThreeDotMenu {
            verifyDesktopSiteModeEnabled(false)
        }
    }
}
