/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.content.Context
import androidx.test.espresso.Espresso.openActionBarOverflowOrOptionsMenu
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.longTapSelectItem
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
        // Clearing all history data after each test to avoid overlapping data
        val applicationContext: Context = activityTestRule.activity.applicationContext
        val historyStorage = PlacesHistoryStorage(applicationContext)
        runBlocking {
            historyStorage.deleteEverything()
        }
    }

    @Test
    @Ignore("Temp disable flakey test - see: https://github.com/mozilla-mobile/fenix/issues/5462")
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
    @Ignore("Temp disable flakey test - see: https://github.com/mozilla-mobile/fenix/issues/5462")
    fun visitedUrlHistoryTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openThreeDotMenu {
        }.openLibrary {
        }.openHistory {
            verifyHistoryMenuView()
            verifyVisitedTimeTitle()
            verifyFirstTestPageTitle("Test_Page_1")
            verifyTestPageUrl(firstWebPage.url)
        }
    }

    @Test
    @Ignore("Temp disable flakey test - see: https://github.com/mozilla-mobile/fenix/issues/5462")
    fun deleteHistoryItemTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openThreeDotMenu {
        }.openLibrary {
        }.openHistory {
            openOverflowMenu()
            clickThreeDotMenuDelete()
            verifyEmptyHistoryView()
        }
    }

    @Test
    fun deleteAllHistoryTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openThreeDotMenu {
        }.openLibrary {
        }.openHistory {
            clickDeleteHistoryButton()
            verifyDeleteConfirmationMessage()
            confirmDeleteAllHistory()
            verifyEmptyHistoryView()
        }
    }

    @Test
    @Ignore("Temp disable flakey test - see: https://github.com/mozilla-mobile/fenix/issues/5462")
    fun multiSelectionToolbarItemsTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openThreeDotMenu {
        }.openLibrary {
        }.openHistory {
            longTapSelectItem(firstWebPage.url)
        }

        multipleSelectionToolbar {
            verifyMultiSelectionCheckmark()
            verifyMultiSelectionCounter()
            verifyShareButton()
            verifyCloseToolbarButton()
        }.closeToolbarReturnToHistory {
            verifyHistoryMenuView()
        }
    }

    @Test
    @Ignore("Temp disable flakey test - see: https://github.com/mozilla-mobile/fenix/issues/5462")
    fun openHistoryInNewTabTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openHomeScreen {
            closeTab()
        }.openThreeDotMenu {
        }.openHistory {
            longTapSelectItem(firstWebPage.url)
            openActionBarOverflowOrOptionsMenu(activityTestRule.getActivity())
        }

        multipleSelectionToolbar {
        }.clickOpenNewTab {
            verifyExistingTabList()
            verifyOpenTabsHeader()
        }
    }

    @Test
    @Ignore("Temp disable flakey test - see: https://github.com/mozilla-mobile/fenix/issues/5462")
    fun openHistoryInPrivateTabTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openThreeDotMenu {
        }.openLibrary {
        }.openHistory {
            longTapSelectItem(firstWebPage.url)
            openActionBarOverflowOrOptionsMenu(activityTestRule.getActivity())
        }

        multipleSelectionToolbar {
        }.clickOpenPrivateTab {
            verifyExistingTabList()
            verifyPrivateSessionHeader()
        }
    }

    @Test
    fun deleteMultipleSelectionTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openHomeScreen {}

        navigationToolbar {
        }.enterURLAndEnterToBrowser(secondWebPage.url) {
        }.openThreeDotMenu {
        }.openLibrary {
        }.openHistory {
            longTapSelectItem(firstWebPage.url)
            longTapSelectItem(secondWebPage.url)
            openActionBarOverflowOrOptionsMenu(activityTestRule.getActivity())
        }

        multipleSelectionToolbar {
        }.clickMultiSelectionDelete {
            verifyEmptyHistoryView()
        }
    }

    @Test
    @Ignore("Temp disable flakey test - see: https://github.com/mozilla-mobile/fenix/issues/5462")
    fun shareButtonTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openThreeDotMenu {
        }.openLibrary {
        }.openHistory {
            longTapSelectItem(firstWebPage.url)
        }

        multipleSelectionToolbar {
            clickShareButton()
            verifyShareOverlay()
            verifyShareTabFavicon()
            verifyShareTabTitle()
            verifyShareTabUrl()
        }
    }

    @Test
    @Ignore("Temp disable flakey test - see: https://github.com/mozilla-mobile/fenix/issues/5462")
    fun verifyBackNavigation() {
        homeScreen {
        }.openThreeDotMenu {
        }.openHistory {
        }.goBack {
            verifyHomeScreen()
        }
    }

    @Test
    @Ignore("Test will be included after back navigation from History Fragment is sorted")
    fun verifyCloseMenu() {
        homeScreen {
        }.openThreeDotMenu {
        }.openHistory {
        }.closeMenu {
            verifyHomeScreen()
        }
    }
}
