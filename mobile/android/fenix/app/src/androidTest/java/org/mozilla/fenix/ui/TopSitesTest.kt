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
import org.mozilla.fenix.R
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.Constants.defaultTopSitesList
import org.mozilla.fenix.helpers.DataGenerationHelper.generateRandomString
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestHelper.clickSnackbarButton
import org.mozilla.fenix.helpers.TestHelper.waitUntilSnackbarGone
import org.mozilla.fenix.ui.robots.browserScreen
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
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityIntentTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides(skipOnboarding = true)

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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/532598
    @SmokeTest
    @Test
    fun addAWebsiteAsATopSiteTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        homeScreen {
            verifyExistingTopSitesList()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            verifyPageContent(defaultWebPage.content)
        }.openThreeDotMenu {
            expandMenu()
            verifyAddToShortcutsButton(shouldExist = true)
        }.addToFirefoxHome {
            verifySnackBarText(getStringResource(R.string.snackbar_added_to_shortcuts))
        }.goToHomescreen {
            verifyExistingTopSitesList()
            verifyExistingTopSitesTabs(defaultWebPage.title)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/532599
    @Test
    fun openTopSiteInANewTabTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        homeScreen {
            verifyExistingTopSitesList()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            verifyPageContent(defaultWebPage.content)
        }.openThreeDotMenu {
            expandMenu()
            verifyAddToShortcutsButton(shouldExist = true)
        }.addToFirefoxHome {
            verifySnackBarText(getStringResource(R.string.snackbar_added_to_shortcuts))
        }.goToHomescreen {
            verifyExistingTopSitesList()
            verifyExistingTopSitesTabs(defaultWebPage.title)
        }.openTopSiteTabWithTitle(title = defaultWebPage.title) {
            verifyUrl(defaultWebPage.url.toString().replace("http://", ""))
        }.goToHomescreen {
            verifyExistingTopSitesList()
            verifyExistingTopSitesTabs(defaultWebPage.title)
        }.openContextMenuOnTopSitesWithTitle(defaultWebPage.title) {
            verifyTopSiteContextMenuItems()
        }

        // Dismiss context menu popup
        mDevice.pressBack()
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/532600
    @Test
    fun openTopSiteInANewPrivateTabTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        homeScreen {
            verifyExistingTopSitesList()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            verifyPageContent(defaultWebPage.content)
        }.openThreeDotMenu {
            expandMenu()
            verifyAddToShortcutsButton(shouldExist = true)
        }.addToFirefoxHome {
            verifySnackBarText(getStringResource(R.string.snackbar_added_to_shortcuts))
        }.goToHomescreen {
            verifyExistingTopSitesList()
            verifyExistingTopSitesTabs(defaultWebPage.title)
        }.openContextMenuOnTopSitesWithTitle(defaultWebPage.title) {
            verifyTopSiteContextMenuItems()
        }.openTopSiteInPrivateTab {
            verifyCurrentPrivateSession(activityIntentTestRule.activity.applicationContext)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1110321
    @Test
    fun renameATopSiteTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)
        val newPageTitle = generateRandomString(5)

        homeScreen {
            verifyExistingTopSitesList()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            waitForPageToLoad()
        }.openThreeDotMenu {
            expandMenu()
            verifyAddToShortcutsButton(shouldExist = true)
        }.addToFirefoxHome {
            verifySnackBarText(getStringResource(R.string.snackbar_added_to_shortcuts))
        }.goToHomescreen {
            verifyExistingTopSitesList()
            verifyExistingTopSitesTabs(defaultWebPage.title)
        }.openContextMenuOnTopSitesWithTitle(defaultWebPage.title) {
            verifyTopSiteContextMenuItems()
        }.renameTopSite(newPageTitle) {
            verifyExistingTopSitesList()
            verifyExistingTopSitesTabs(newPageTitle)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/532601
    @Test
    fun removeTopSiteUsingMenuButtonTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        homeScreen {
            verifyExistingTopSitesList()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            verifyPageContent(defaultWebPage.content)
        }.openThreeDotMenu {
            expandMenu()
            verifyAddToShortcutsButton(shouldExist = true)
        }.addToFirefoxHome {
            verifySnackBarText(getStringResource(R.string.snackbar_added_to_shortcuts))
        }.goToHomescreen {
            verifyExistingTopSitesList()
            verifyExistingTopSitesTabs(defaultWebPage.title)
        }.openContextMenuOnTopSitesWithTitle(defaultWebPage.title) {
            verifyTopSiteContextMenuItems()
        }.removeTopSite {
            clickSnackbarButton("UNDO")
            verifyExistingTopSitesTabs(defaultWebPage.title)
        }.openContextMenuOnTopSitesWithTitle(defaultWebPage.title) {
            verifyTopSiteContextMenuItems()
        }.removeTopSite {
            verifyNotExistingTopSitesList(defaultWebPage.title)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2323641
    @Test
    fun removeTopSiteFromMainMenuTest() {
        val defaultWebPage = getGenericAsset(mockWebServer, 1)

        homeScreen {
            verifyExistingTopSitesList()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            verifyPageContent(defaultWebPage.content)
        }.openThreeDotMenu {
            expandMenu()
            verifyAddToShortcutsButton(shouldExist = true)
        }.addToFirefoxHome {
            verifySnackBarText(getStringResource(R.string.snackbar_added_to_shortcuts))
        }.goToHomescreen {
            verifyExistingTopSitesList()
            verifyExistingTopSitesTabs(defaultWebPage.title)
        }.openTopSiteTabWithTitle(defaultWebPage.title) {
        }.openThreeDotMenu {
            verifyRemoveFromShortcutsButton()
        }.clickRemoveFromShortcuts {
        }.goToHomescreen {
            verifyNotExistingTopSitesList(defaultWebPage.title)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/561582
    // Expected for en-us defaults
    @Test
    fun verifyENLocalesDefaultTopSitesListTest() {
        homeScreen {
            verifyExistingTopSitesList()
            defaultTopSitesList.values.forEach { value ->
                verifyExistingTopSitesTabs(value)
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1050642
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
        }.goToHomescreen {
            verifyExistingTopSitesList()
            verifyExistingTopSitesTabs(defaultWebPage.title)
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
