/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.content.Context
import android.hardware.camera2.CameraManager
import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.test.espresso.Espresso.openActionBarOverflowOrOptionsMenu
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.icons.generator.DefaultIconGenerator
import mozilla.components.feature.search.ext.createSearchEngine
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.Constants.PackageName.ANDROID_SETTINGS
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.SearchDispatcher
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestHelper.appContext
import org.mozilla.fenix.helpers.TestHelper.assertNativeAppOpens
import org.mozilla.fenix.helpers.TestHelper.denyPermission
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.helpers.TestHelper.grantPermission
import org.mozilla.fenix.helpers.TestHelper.longTapSelectItem
import org.mozilla.fenix.helpers.TestHelper.setCustomSearchEngine
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.multipleSelectionToolbar

/**
 *  Tests for verifying the search fragment
 *
 *  Including:
 * - Verify the toolbar, awesomebar, and shortcut bar are displayed
 * - Select shortcut button
 * - Select scan button
 *
 */

class SearchTest {
    lateinit var searchMockServer: MockWebServer

    @get:Rule
    val activityTestRule = AndroidComposeTestRule(
        HomeActivityTestRule(
            isPocketEnabled = false,
            isJumpBackInCFREnabled = false,
            isTCPCFREnabled = false,
            isWallpaperOnboardingEnabled = false,
        ),
    ) { it.activity }

    @Before
    fun setUp() {
        searchMockServer = MockWebServer().apply {
            dispatcher = SearchDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        searchMockServer.shutdown()
    }

    @Test
    fun searchScreenItemsTest() {
        homeScreen {
        }.openSearch {
            verifySearchView()
            verifyBrowserToolbar()
            verifyScanButton()
            verifySearchEngineButton()
        }
    }

    @SmokeTest
    @Test
    fun scanButtonDenyPermissionTest() {
        val cameraManager = appContext.getSystemService(Context.CAMERA_SERVICE) as CameraManager
        assumeTrue(cameraManager.cameraIdList.isNotEmpty())

        homeScreen {
        }.openSearch {
            clickScanButton()
            denyPermission()
            clickScanButton()
            clickDismissPermissionRequiredDialog()
        }
        homeScreen {
        }.openSearch {
            clickScanButton()
            clickGoToPermissionsSettings()
            assertNativeAppOpens(ANDROID_SETTINGS)
        }
    }

    @SmokeTest
    @Test
    fun scanButtonAllowPermissionTest() {
        val cameraManager = appContext.getSystemService(Context.CAMERA_SERVICE) as CameraManager
        assumeTrue(cameraManager.cameraIdList.isNotEmpty())

        homeScreen {
        }.openSearch {
            clickScanButton()
            grantPermission()
            verifyScannerOpen()
        }
    }

    @Test
    fun setDefaultSearchEngineFromShortcutsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            toggleShowSearchShortcuts()
        }.goBack {
        }.goBack {
        }.openSearch {
            scrollToSearchEngineSettings(activityTestRule)
        }.clickSearchEngineSettings(activityTestRule) {
            changeDefaultSearchEngine("DuckDuckGo")
        }

        exitMenu()

        homeScreen {
        }.openSearch {
        }.submitQuery("firefox") {
            verifyUrl("duckduckgo.com/?q=firefox")
        }
    }

    @Test
    fun clearSearchTest() {
        homeScreen {
        }.openSearch {
            typeSearch("test")
            clickClearButton()
            verifySearchBarEmpty()
        }
    }

    @Ignore("Failure caused by bugs: https://github.com/mozilla-mobile/fenix/issues/23818")
    @SmokeTest
    @Test
    fun searchGroupShowsInRecentlyVisitedTest() {
        val firstPage = getGenericAsset(searchMockServer, 1)
        val secondPage = getGenericAsset(searchMockServer, 2)
        // setting our custom mockWebServer search URL
        val searchString =
            "http://localhost:${searchMockServer.port}/searchResults.html?search={searchTerms}"
        val customSearchEngine = createSearchEngine(
            name = "TestSearchEngine",
            url = searchString,
            icon = DefaultIconGenerator().generate(appContext, IconRequest(searchString)).bitmap,
        )
        setCustomSearchEngine(customSearchEngine)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery("test search") {
            longClickLink("Link 1")
            clickContextOpenLinkInNewTab()
            longClickLink("Link 2")
            clickContextOpenLinkInNewTab()
        }.openTabDrawer {
        }.openTab(firstPage.title) {
        }.openTabDrawer {
        }.openTab(secondPage.title) {
        }.openTabDrawer {
        }.openTabsListThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(true, "test search", 3)
        }
    }

    @SmokeTest
    @Test
    fun noSearchGroupFromPrivateBrowsingTest() {
        // setting our custom mockWebServer search URL
        val searchString = "http://localhost:${searchMockServer.port}/searchResults.html?search={searchTerms}"
        val customSearchEngine = createSearchEngine(
            name = "TestSearchEngine",
            url = searchString,
            icon = DefaultIconGenerator().generate(appContext, IconRequest(searchString)).bitmap,
        )
        setCustomSearchEngine(customSearchEngine)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery("test search") {
            longClickLink("Link 1")
            clickContextOpenLinkInPrivateTab()
            longClickLink("Link 2")
            clickContextOpenLinkInPrivateTab()
        }.openTabDrawer {
        }.toggleToPrivateTabs {
        }.openTabWithIndex(0) {
        }.openTabDrawer {
        }.openTabWithIndex(1) {
        }.openTabDrawer {
        }.openTabsListThreeDotMenu {
        }.closeAllTabs {
            togglePrivateBrowsingModeOnOff()
            verifyCurrentSearchGroupIsDisplayed(false, "test search", 3)
        }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryItemExists(false, "3 sites")
        }
    }

    @Ignore("Failure caused by bugs: https://github.com/mozilla-mobile/fenix/issues/23818")
    @SmokeTest
    @Test
    fun deleteItemsFromSearchGroupsHistoryTest() {
        val firstPage = getGenericAsset(searchMockServer, 1)
        val secondPage = getGenericAsset(searchMockServer, 2)
        // setting our custom mockWebServer search URL
        val searchString = "http://localhost:${searchMockServer.port}/searchResults.html?search={searchTerms}"
        val customSearchEngine = createSearchEngine(
            name = "TestSearchEngine",
            url = searchString,
            icon = DefaultIconGenerator().generate(appContext, IconRequest(searchString)).bitmap,
        )
        setCustomSearchEngine(customSearchEngine)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery("test search") {
            longClickLink("Link 1")
            clickContextOpenLinkInNewTab()
            longClickLink("Link 2")
            clickContextOpenLinkInNewTab()
        }.openTabDrawer {
        }.openTab(firstPage.title) {
        }.openTabDrawer {
        }.openTab(secondPage.title) {
        }.openTabDrawer {
        }.openTabsListThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(true, "test search", 3)
        }.openRecentlyVisitedSearchGroupHistoryList("test search") {
            clickDeleteHistoryButton(firstPage.url.toString())
            longTapSelectItem(secondPage.url)
            multipleSelectionToolbar {
                openActionBarOverflowOrOptionsMenu(activityTestRule.activity)
                clickMultiSelectionDelete()
            }
            exitMenu()
        }
        homeScreen {
            // checking that the group is removed when only 1 item is left
            verifyRecentlyVisitedSearchGroupDisplayed(false, "test search", 1)
        }
    }
}
