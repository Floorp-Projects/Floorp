/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.test.espresso.Espresso.openActionBarOverflowOrOptionsMenu
import androidx.test.espresso.intent.Intents
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.RecyclerViewIdlingResource
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestHelper.longTapSelectItem
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.registerAndCleanupIdlingResources
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying basic functionality of recently closed tabs history
 *
 */
class RecentlyClosedTabsTest {
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityTestRule = AndroidComposeTestRule(
        HomeActivityTestRule.withDefaultSettingsOverrides(
            tabsTrayRewriteEnabled = true,
        ),
    ) { it.activity }

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }

        Intents.init()
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    // This test verifies the Recently Closed Tabs List and items
    @Test
    fun verifyRecentlyClosedTabsListTest() {
        val website = getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(website.url) {
            mDevice.waitForIdle()
        }.openComposeTabDrawer(activityTestRule) {
            closeTab()
        }
        homeScreen {
        }.openThreeDotMenu {
        }.openHistory {
        }.openRecentlyClosedTabs {
            waitForListToExist()
            registerAndCleanupIdlingResources(
                RecyclerViewIdlingResource(
                    activityTestRule.activity.findViewById(R.id.recently_closed_list),
                    1,
                ),
            ) {
                verifyRecentlyClosedTabsMenuView()
            }
            verifyRecentlyClosedTabsPageTitle("Test_Page_1")
            verifyRecentlyClosedTabsUrl(website.url)
        }
    }

    // Verifies that a recently closed item is properly opened
    @SmokeTest
    @Test
    fun openRecentlyClosedItemTest() {
        val website = getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(website.url) {
            mDevice.waitForIdle()
        }.openComposeTabDrawer(activityTestRule) {
            closeTab()
        }
        homeScreen {
        }.openThreeDotMenu {
        }.openHistory {
        }.openRecentlyClosedTabs {
            waitForListToExist()
            registerAndCleanupIdlingResources(
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.recently_closed_list), 1),
            ) {
                verifyRecentlyClosedTabsMenuView()
            }
        }.clickRecentlyClosedItem("Test_Page_1") {
            verifyUrl(website.url.toString())
        }
    }

    // Verifies that tapping the "x" button removes a recently closed item from the list
    @SmokeTest
    @Test
    fun deleteRecentlyClosedTabsItemTest() {
        val website = getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(website.url) {
            mDevice.waitForIdle()
        }.openComposeTabDrawer(activityTestRule) {
            closeTab()
        }
        homeScreen {
        }.openThreeDotMenu {
        }.openHistory {
        }.openRecentlyClosedTabs {
            waitForListToExist()
            registerAndCleanupIdlingResources(
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.recently_closed_list), 1),
            ) {
                verifyRecentlyClosedTabsMenuView()
            }
            clickDeleteRecentlyClosedTabs()
            verifyEmptyRecentlyClosedTabsList()
        }
    }

    @Test
    fun openInNewTabRecentlyClosedTabsTest() {
        val firstPage = getGenericAsset(mockWebServer, 1)
        val secondPage = getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstPage.url) {
            waitForPageToLoad()
        }.openComposeTabDrawer(activityTestRule) {
        }.openNewTab {
        }.submitQuery(secondPage.url.toString()) {
            waitForPageToLoad()
        }.openComposeTabDrawer(activityTestRule) {
        }.openThreeDotMenu {
        }.closeAllTabs {
        }.openThreeDotMenu {
        }.openHistory {
        }.openRecentlyClosedTabs {
            waitForListToExist()
            longTapSelectItem(firstPage.url)
            longTapSelectItem(secondPage.url)
            openActionBarOverflowOrOptionsMenu(activityTestRule.activity)
        }.clickOpenInNewTab(activityTestRule) {
            // URL verification to be removed once https://bugzilla.mozilla.org/show_bug.cgi?id=1839179 is fixed.
            browserScreen {
                verifyPageContent(secondPage.content)
                verifyUrl(secondPage.url.toString())
            }.openComposeTabDrawer(activityTestRule) {
                verifyNormalBrowsingButtonIsSelected(true)
                verifyExistingOpenTabs(firstPage.title, secondPage.title)
            }
        }
    }

    @Test
    fun openInPrivateTabRecentlyClosedTabsTest() {
        val firstPage = getGenericAsset(mockWebServer, 1)
        val secondPage = getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstPage.url) {
            waitForPageToLoad()
        }.openComposeTabDrawer(activityTestRule) {
        }.openNewTab {
        }.submitQuery(secondPage.url.toString()) {
            waitForPageToLoad()
        }.openComposeTabDrawer(activityTestRule) {
        }.openThreeDotMenu {
        }.closeAllTabs {
        }.openThreeDotMenu {
        }.openHistory {
        }.openRecentlyClosedTabs {
            waitForListToExist()
            longTapSelectItem(firstPage.url)
            longTapSelectItem(secondPage.url)
            openActionBarOverflowOrOptionsMenu(activityTestRule.activity)
        }.clickOpenInPrivateTab(activityTestRule) {
            // URL verification to be removed once https://bugzilla.mozilla.org/show_bug.cgi?id=1839179 is fixed.
            browserScreen {
                verifyPageContent(secondPage.content)
                verifyUrl(secondPage.url.toString())
            }.openComposeTabDrawer(activityTestRule) {
                verifyPrivateBrowsingButtonIsSelected(true)
                verifyExistingOpenTabs(firstPage.title, secondPage.title)
            }
        }
    }

    @Test
    fun shareMultipleRecentlyClosedTabsTest() {
        val firstPage = getGenericAsset(mockWebServer, 1)
        val secondPage = getGenericAsset(mockWebServer, 2)
        val sharingApp = "Gmail"
        val urlString = "${firstPage.url}\n\n${secondPage.url}"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstPage.url) {
            waitForPageToLoad()
        }.openComposeTabDrawer(activityTestRule) {
        }.openNewTab {
        }.submitQuery(secondPage.url.toString()) {
            waitForPageToLoad()
        }.openComposeTabDrawer(activityTestRule) {
        }.openThreeDotMenu {
        }.closeAllTabs {
        }.openThreeDotMenu {
        }.openHistory {
        }.openRecentlyClosedTabs {
            waitForListToExist()
            longTapSelectItem(firstPage.url)
            longTapSelectItem(secondPage.url)
        }.clickShare {
            verifyShareTabsOverlay(firstPage.title, secondPage.title)
            verifySharingWithSelectedApp(sharingApp, urlString, "${firstPage.title}, ${secondPage.title}")
        }
    }

    @Test
    fun privateBrowsingNotSavedInRecentlyClosedTabsTest() {
        val firstPage = getGenericAsset(mockWebServer, 1)
        val secondPage = getGenericAsset(mockWebServer, 2)

        homeScreen {}.togglePrivateBrowsingMode()
        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstPage.url) {
            waitForPageToLoad()
        }.openComposeTabDrawer(activityTestRule) {
        }.openNewTab {
        }.submitQuery(secondPage.url.toString()) {
            waitForPageToLoad()
        }.openComposeTabDrawer(activityTestRule) {
        }.openThreeDotMenu {
        }.closeAllTabs {
        }.openThreeDotMenu {
        }.openHistory {
        }.openRecentlyClosedTabs {
            verifyEmptyRecentlyClosedTabsList()
        }
    }

    @Test
    fun deleteHistoryClearsRecentlyClosedTabsListTest() {
        val firstPage = getGenericAsset(mockWebServer, 1)
        val secondPage = getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstPage.url) {
            waitForPageToLoad()
        }.openComposeTabDrawer(activityTestRule) {
        }.openNewTab {
        }.submitQuery(secondPage.url.toString()) {
            waitForPageToLoad()
        }.openComposeTabDrawer(activityTestRule) {
        }.openThreeDotMenu {
        }.closeAllTabs {
        }.openThreeDotMenu {
        }.openHistory {
        }.openRecentlyClosedTabs {
            waitForListToExist()
        }.goBackToHistoryMenu {
            clickDeleteAllHistoryButton()
            selectEverythingOption()
            confirmDeleteAllHistory()
            verifyEmptyHistoryView()
        }.openRecentlyClosedTabs {
            verifyEmptyRecentlyClosedTabsList()
        }
    }
}
