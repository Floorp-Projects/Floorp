/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestHelper.clickSnackbarButton
import org.mozilla.fenix.helpers.TestHelper.generateRandomString
import org.mozilla.fenix.helpers.TestHelper.getStringResource
import org.mozilla.fenix.helpers.TestHelper.waitUntilSnackbarGone
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.homeScreenWithComposeTopSites
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 * Tests Top Sites functionality
 *
 * - Verifies 'Add to Firefox Home' UI functionality
 * - Verifies 'Top Sites' context menu UI functionality
 * - Verifies 'Top Site' usage UI functionality
 * - Verifies existence of default top sites available on the home-screen
 */

class ComposeTopSitesTest {
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val composeTestRule =
        AndroidComposeTestRule(
            HomeActivityTestRule.withDefaultSettingsOverrides(
                composeTopSitesEnabled = true,
            ),
        ) { it.activity }

    @Before
    fun setUp() {
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    @SmokeTest
    @Test
    fun addAWebsiteAsATopSiteTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
            expandMenu()
            verifyAddToShortcutsButton(true)
        }.addToFirefoxHome {
            verifySnackBarText(getStringResource(R.string.snackbar_added_to_shortcuts))
        }.goToHomescreenWithComposeTopSites(composeTestRule) {
            verifyExistingTopSitesList()
            verifyExistingTopSiteItem(defaultWebPage.title)
        }
    }

    @Test
    fun openTopSiteInANewTabTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
            expandMenu()
            verifyAddToShortcutsButton(true)
        }.addToFirefoxHome {
            verifySnackBarText(getStringResource(R.string.snackbar_added_to_shortcuts))
        }.goToHomescreenWithComposeTopSites(composeTestRule) {
            verifyExistingTopSitesList()
            verifyExistingTopSiteItem(defaultWebPage.title)
        }.openTopSiteTabWithTitle(title = defaultWebPage.title) {
            verifyUrl(defaultWebPage.url.toString().replace("http://", ""))
        }.goToHomescreenWithComposeTopSites(composeTestRule) {
            verifyExistingTopSitesList()
            verifyExistingTopSiteItem(defaultWebPage.title)
        }.openContextMenuOnTopSitesWithTitle(defaultWebPage.title) {
            verifyTopSiteContextMenuItems()
        }

        // Dismiss context menu popup
        mDevice.pressBack()
    }

    @Test
    fun openTopSiteInANewPrivateTabTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
            expandMenu()
            verifyAddToShortcutsButton(true)
        }.addToFirefoxHome {
            verifySnackBarText(getStringResource(R.string.snackbar_added_to_shortcuts))
        }.goToHomescreenWithComposeTopSites(composeTestRule) {
            verifyExistingTopSitesList()
            verifyExistingTopSiteItem(defaultWebPage.title)
        }.openContextMenuOnTopSitesWithTitle(defaultWebPage.title) {
            verifyTopSiteContextMenuItems()
        }.openTopSiteInPrivate() {
            verifyCurrentPrivateSession(composeTestRule.activity.applicationContext)
        }
    }

    @Test
    fun renameATopSiteTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)
        val newPageTitle = generateRandomString(5)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            waitForPageToLoad()
        }.openThreeDotMenu {
            expandMenu()
            verifyAddToShortcutsButton(true)
        }.addToFirefoxHome {
            verifySnackBarText(getStringResource(R.string.snackbar_added_to_shortcuts))
        }.goToHomescreenWithComposeTopSites(composeTestRule) {
            verifyExistingTopSitesList()
            verifyExistingTopSiteItem(defaultWebPage.title)
        }.openContextMenuOnTopSitesWithTitle(defaultWebPage.title) {
            verifyTopSiteContextMenuItems()
        }.renameTopSite(newPageTitle) {
            verifyExistingTopSitesList()
            verifyExistingTopSiteItem(newPageTitle)
        }
    }

    @Test
    fun removeTopSiteUsingMenuButtonTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
            expandMenu()
            verifyAddToShortcutsButton(true)
        }.addToFirefoxHome {
            verifySnackBarText(getStringResource(R.string.snackbar_added_to_shortcuts))
        }.goToHomescreenWithComposeTopSites(composeTestRule) {
            verifyExistingTopSitesList()
            verifyExistingTopSiteItem(defaultWebPage.title)
        }.openContextMenuOnTopSitesWithTitle(defaultWebPage.title) {
            verifyTopSiteContextMenuItems()
        }.removeTopSite {
            clickSnackbarButton("UNDO")
            verifyExistingTopSiteItem(defaultWebPage.title)
        }.openContextMenuOnTopSitesWithTitle(defaultWebPage.title) {
            verifyTopSiteContextMenuItems()
        }.removeTopSite {
            verifyNotExistingTopSiteItem(defaultWebPage.title)
        }
    }

    @Test
    fun removeTopSiteFromMainMenuTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
            expandMenu()
            verifyAddToShortcutsButton(true)
        }.addToFirefoxHome {
            verifySnackBarText(getStringResource(R.string.snackbar_added_to_shortcuts))
        }.goToHomescreenWithComposeTopSites(composeTestRule) {
            verifyExistingTopSitesList()
            verifyExistingTopSiteItem(defaultWebPage.title)
        }.openTopSiteTabWithTitle(defaultWebPage.title) {
        }.openThreeDotMenu {
            verifyRemoveFromShortcutsButton()
        }.clickRemoveFromShortcuts {
        }.goToHomescreenWithComposeTopSites(composeTestRule) {
            verifyNotExistingTopSiteItem(defaultWebPage.title)
        }
    }

    // Expected for en-us defaults
    @Test
    fun verifyENLocalesDefaultTopSitesListTest() {
        homeScreenWithComposeTopSites(composeTestRule) {
            verifyExistingTopSitesList()
            val topSitesTitles = arrayListOf("Google", "Top Articles", "Wikipedia")
            topSitesTitles.forEach { value ->
                verifyExistingTopSiteItem(value)
            }
        }
    }

    @SmokeTest
    @Test
    fun addAndRemoveMostViewedTopSiteTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        for (i in 0..1) {
            navigationToolbar {
            }.enterURLAndEnterToBrowser(defaultWebPage.url) {
                waitForPageToLoad()
            }
        }

        browserScreen {
        }.goToHomescreenWithComposeTopSites(composeTestRule) {
            verifyExistingTopSitesList()
            verifyExistingTopSiteItem(defaultWebPage.title)
        }.openContextMenuOnTopSitesWithTitle(defaultWebPage.title) {
        }.deleteTopSiteFromHistory {
            verifySnackBarText(getStringResource(R.string.snackbar_top_site_removed))
            waitUntilSnackbarGone()
        }.openThreeDotMenu {
        }.openHistory {
            verifyEmptyHistoryView()
        }
    }
}
