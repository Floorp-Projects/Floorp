/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.content.Context
import androidx.test.espresso.Espresso.openActionBarOverflowOrOptionsMenu
import androidx.test.espresso.IdlingRegistry
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.RecyclerViewIdlingResource
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.longTapSelectItem
import org.mozilla.fenix.ui.robots.historyMenu
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.multipleSelectionToolbar
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying basic functionality of history
 *
 */
class HistoryTest {
    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.
    private lateinit var mockWebServer: MockWebServer
    private lateinit var mDevice: UiDevice
    private var historyListIdlingResource: RecyclerViewIdlingResource? = null
    private var recentlyClosedTabsListIdlingResource: RecyclerViewIdlingResource? = null

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

    @Before
    fun setUp() {
        InstrumentationRegistry.getInstrumentation().targetContext.settings()
            .shouldShowJumpBackInCFR = false

        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
        // Clearing all history data after each test to avoid overlapping data
        val applicationContext: Context = activityTestRule.activity.applicationContext
        val historyStorage = PlacesHistoryStorage(applicationContext)

        runBlocking {
            historyStorage.deleteEverything()
        }

        if (historyListIdlingResource != null) {
            IdlingRegistry.getInstance().unregister(historyListIdlingResource!!)
        }

        if (recentlyClosedTabsListIdlingResource != null) {
            IdlingRegistry.getInstance().unregister(recentlyClosedTabsListIdlingResource!!)
        }
    }

    @Test
    fun noHistoryItemsInCacheTest() {
        homeScreen {
        }.openThreeDotMenu {
            verifyHistoryButton()
        }.openHistory {
            verifyHistoryMenuView()
            verifyEmptyHistoryView()
        }
    }

    @Test
    // Test running on beta/release builds in CI:
    // caution when making changes to it, so they don't block the builds
    fun visitedUrlHistoryTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryListExists()
            historyListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.history_list), 1)
            IdlingRegistry.getInstance().register(historyListIdlingResource!!)
            verifyHistoryMenuView()
            verifyVisitedTimeTitle()
            verifyFirstTestPageTitle("Test_Page_1")
            verifyTestPageUrl(firstWebPage.url)
        }
    }

    @Test
    fun deleteHistoryItemTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryListExists()
            historyListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.history_list), 1)
            IdlingRegistry.getInstance().register(historyListIdlingResource!!)
            clickDeleteHistoryButton(firstWebPage.url.toString())
            IdlingRegistry.getInstance().unregister(historyListIdlingResource!!)
            verifyDeleteSnackbarText("Deleted")
            verifyEmptyHistoryView()
        }
    }

    @SmokeTest
    @Test
    fun deleteAllHistoryTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryListExists()
            historyListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.history_list), 1)
            IdlingRegistry.getInstance().register(historyListIdlingResource!!)
            clickDeleteAllHistoryButton()
            IdlingRegistry.getInstance().unregister(historyListIdlingResource!!)
            verifyDeleteConfirmationMessage()
            confirmDeleteAllHistory()
            verifyDeleteSnackbarText("Browsing data deleted")
            verifyEmptyHistoryView()
        }
    }

    @SmokeTest
    @Test
    fun historyMultiSelectionToolbarItemsTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryListExists()
            historyListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.history_list), 1)
            IdlingRegistry.getInstance().register(historyListIdlingResource!!)
            longTapSelectItem(firstWebPage.url)
        }

        multipleSelectionToolbar {
            verifyMultiSelectionCheckmark()
            verifyMultiSelectionCounter()
            verifyShareHistoryButton()
            verifyCloseToolbarButton()
        }.closeToolbarReturnToHistory {
            verifyHistoryMenuView()
        }
    }

    @Test
    fun openHistoryInNewTabTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            mDevice.waitForIdle()
        }.openTabDrawer {
            closeTab()
        }

        homeScreen { }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryListExists()
            historyListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.history_list), 1)
            IdlingRegistry.getInstance().register(historyListIdlingResource!!)
            longTapSelectItem(firstWebPage.url)
            openActionBarOverflowOrOptionsMenu(activityTestRule.activity)
        }

        multipleSelectionToolbar {
        }.clickOpenNewTab {
            verifyExistingTabList()
            verifyNormalModeSelected()
        }
    }

    @Test
    fun openHistoryInPrivateTabTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryListExists()
            historyListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.history_list), 1)
            IdlingRegistry.getInstance().register(historyListIdlingResource!!)
            longTapSelectItem(firstWebPage.url)
            openActionBarOverflowOrOptionsMenu(activityTestRule.activity)
        }

        multipleSelectionToolbar {
        }.clickOpenPrivateTab {
            verifyPrivateModeSelected()
            verifyExistingTabList()
        }
    }

    @Test
    fun deleteMultipleSelectionTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(secondWebPage.url) {
            mDevice.waitForIdle()
            verifyUrl(secondWebPage.url.toString())
        }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryListExists()
            historyListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.history_list), 2)
            IdlingRegistry.getInstance().register(historyListIdlingResource!!)
            verifyHistoryItemExists(true, firstWebPage.url.toString())
            verifyHistoryItemExists(true, secondWebPage.url.toString())
            longTapSelectItem(firstWebPage.url)
            longTapSelectItem(secondWebPage.url)
            openActionBarOverflowOrOptionsMenu(activityTestRule.activity)
            IdlingRegistry.getInstance().unregister(historyListIdlingResource!!)
        }

        multipleSelectionToolbar {
            clickMultiSelectionDelete()
        }

        historyMenu {
            verifyEmptyHistoryView()
        }
    }

    @Test
    fun shareButtonTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryListExists()
            historyListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.history_list), 1)
            IdlingRegistry.getInstance().register(historyListIdlingResource!!)
            longTapSelectItem(firstWebPage.url)
        }

        multipleSelectionToolbar {
            clickShareHistoryButton()
            verifyShareOverlay()
            verifyShareTabFavicon()
            verifyShareTabTitle()
            verifyShareTabUrl()
        }
    }

    @Test
    // This test verifies the Recently Closed Tabs List and items
    fun verifyRecentlyClosedTabsListTest() {
        val website = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(website.url) {
            mDevice.waitForIdle()
        }.openTabDrawer {
            closeTab()
        }.openTabDrawer {
        }.openRecentlyClosedTabs {
            waitForListToExist()
            recentlyClosedTabsListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.recently_closed_list), 1)
            IdlingRegistry.getInstance().register(recentlyClosedTabsListIdlingResource!!)
            verifyRecentlyClosedTabsMenuView()
            IdlingRegistry.getInstance().unregister(recentlyClosedTabsListIdlingResource!!)
            verifyRecentlyClosedTabsPageTitle("Test_Page_1")
            verifyRecentlyClosedTabsUrl(website.url)
        }
    }
}
