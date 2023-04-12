/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.content.Context
import android.hardware.camera2.CameraManager
import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.core.net.toUri
import androidx.test.espresso.Espresso.openActionBarOverflowOrOptionsMenu
import androidx.test.espresso.Espresso.pressBack
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
import org.mozilla.fenix.helpers.Constants.searchEngineCodes
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.SearchDispatcher
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestHelper.appContext
import org.mozilla.fenix.helpers.TestHelper.assertNativeAppOpens
import org.mozilla.fenix.helpers.TestHelper.denyPermission
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.helpers.TestHelper.grantSystemPermission
import org.mozilla.fenix.helpers.TestHelper.longTapSelectItem
import org.mozilla.fenix.helpers.TestHelper.mDevice
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
    lateinit var queryString: String

    @get:Rule
    val activityTestRule = AndroidComposeTestRule(
        HomeActivityTestRule(
            skipOnboarding = true,
            isPocketEnabled = false,
            isJumpBackInCFREnabled = false,
            isRecentTabsFeatureEnabled = false,
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
            grantSystemPermission()
            verifyScannerOpen()
        }
    }

    @Test
    fun setDefaultSearchEngineFromShortcutsTest() {
        queryString = "firefox"

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
        }.submitQuery(queryString) {
            verifyUrl("duckduckgo.com/?q=firefox")
        }
    }

    @Test
    fun clearSearchTest() {
        queryString = "test"

        homeScreen {
        }.openSearch {
            typeSearch(queryString)
            clickClearButton()
            verifySearchBarEmpty()
        }
    }

    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @SmokeTest
    @Test
    fun searchGroupShowsInRecentlyVisitedTest() {
        queryString = "test search"
        // setting our custom mockWebServer search URL
        val searchString =
            "http://localhost:${searchMockServer.port}/pages/searchResults.html?search={searchTerms}"
        val customSearchEngine = createSearchEngine(
            name = "TestSearchEngine",
            url = searchString,
            icon = DefaultIconGenerator().generate(appContext, IconRequest(searchString)).bitmap,
        )
        setCustomSearchEngine(customSearchEngine)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickLink("Link 1")
            clickContextOpenLinkInNewTab()
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            pressBack()
            longClickLink("Link 2")
            clickContextOpenLinkInNewTab()
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
        }.openTabDrawer {
        }.openTabsListThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(true, queryString, 3)
        }
    }

    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @Test
    fun verifySearchGroupHistoryWithNoDuplicatesTest() {
        val firstPageUrl = getGenericAsset(searchMockServer, 1).url
        val secondPageUrl = getGenericAsset(searchMockServer, 2).url
        val originPageUrl =
            "http://localhost:${searchMockServer.port}/pages/searchResults.html?search=test%20search".toUri()
        queryString = "test search"
        // setting our custom mockWebServer search URL
        val searchString =
            "http://localhost:${searchMockServer.port}/pages/searchResults.html?search={searchTerms}"
        val customSearchEngine = createSearchEngine(
            name = "TestSearchEngine",
            url = searchString,
            icon = DefaultIconGenerator().generate(appContext, IconRequest(searchString)).bitmap,
        )
        setCustomSearchEngine(customSearchEngine)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickLink("Link 1")
            clickContextOpenLinkInNewTab()
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            pressBack()
            longClickLink("Link 1")
            clickContextOpenLinkInNewTab()
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            pressBack()
            longClickLink("Link 2")
            clickContextOpenLinkInNewTab()
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            pressBack()
            longClickLink("Link 1")
            clickContextOpenLinkInNewTab()
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
        }.openTabDrawer {
        }.openTabsListThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(true, queryString, 3)
        }.openRecentlyVisitedSearchGroupHistoryList(queryString) {
            verifyTestPageUrl(firstPageUrl)
            verifyTestPageUrl(secondPageUrl)
            verifyTestPageUrl(originPageUrl)
        }
    }

    @Ignore("Failing due to known bug, see https://github.com/mozilla-mobile/fenix/issues/23818")
    @Test
    fun searchGroupGeneratedInTheSameTabTest() {
        queryString = "test search"
        // setting our custom mockWebServer search URL
        val searchString =
            "http://localhost:${searchMockServer.port}/pages/searchResults.html?search={searchTerms}"
        val customSearchEngine = createSearchEngine(
            name = "TestSearchEngine",
            url = searchString,
            icon = DefaultIconGenerator().generate(appContext, IconRequest(searchString)).bitmap,
        )
        setCustomSearchEngine(customSearchEngine)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            clickLinkMatchingText("Link 1")
            waitForPageToLoad()
            pressBack()
            clickLinkMatchingText("Link 2")
            waitForPageToLoad()
        }.openTabDrawer {
        }.openTabsListThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(true, queryString, 3)
        }
    }

    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @SmokeTest
    @Test
    fun noSearchGroupFromPrivateBrowsingTest() {
        queryString = "test search"
        // setting our custom mockWebServer search URL
        val searchString =
            "http://localhost:${searchMockServer.port}/pages/searchResults.html?search={searchTerms}"
        val customSearchEngine = createSearchEngine(
            name = "TestSearchEngine",
            url = searchString,
            icon = DefaultIconGenerator().generate(appContext, IconRequest(searchString)).bitmap,
        )
        setCustomSearchEngine(customSearchEngine)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
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
            verifyRecentlyVisitedSearchGroupDisplayed(false, queryString, 3)
        }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryItemExists(false, "3 sites")
        }
    }

    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @SmokeTest
    @Test
    fun deleteItemsFromSearchGroupHistoryTest() {
        queryString = "test search"
        val firstPageUrl = getGenericAsset(searchMockServer, 1).url
        val secondPageUrl = getGenericAsset(searchMockServer, 2).url
        // setting our custom mockWebServer search URL
        val searchString =
            "http://localhost:${searchMockServer.port}/pages/searchResults.html?search={searchTerms}"
        val customSearchEngine = createSearchEngine(
            name = "TestSearchEngine",
            url = searchString,
            icon = DefaultIconGenerator().generate(appContext, IconRequest(searchString)).bitmap,
        )
        setCustomSearchEngine(customSearchEngine)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickLink("Link 1")
            clickContextOpenLinkInNewTab()
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            mDevice.pressBack()
            longClickLink("Link 2")
            clickContextOpenLinkInNewTab()
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
        }.openTabDrawer {
        }.openTabsListThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(true, queryString, 3)
        }.openRecentlyVisitedSearchGroupHistoryList(queryString) {
            clickDeleteHistoryButton(firstPageUrl.toString())
            longTapSelectItem(secondPageUrl)
            multipleSelectionToolbar {
                openActionBarOverflowOrOptionsMenu(activityTestRule.activity)
                clickMultiSelectionDelete()
            }
            exitMenu()
        }
        homeScreen {
            // checking that the group is removed when only 1 item is left
            verifyRecentlyVisitedSearchGroupDisplayed(false, queryString, 1)
        }
    }

    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @Test
    fun deleteSearchGroupFromHistoryTest() {
        queryString = "test search"
        val firstPageUrl = getGenericAsset(searchMockServer, 1).url
        // setting our custom mockWebServer search URL
        val searchString =
            "http://localhost:${searchMockServer.port}/pages/searchResults.html?search={searchTerms}"
        val customSearchEngine = createSearchEngine(
            name = "TestSearchEngine",
            url = searchString,
            icon = DefaultIconGenerator().generate(appContext, IconRequest(searchString)).bitmap,
        )
        setCustomSearchEngine(customSearchEngine)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickLink("Link 1")
            clickContextOpenLinkInNewTab()
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            mDevice.pressBack()
            longClickLink("Link 2")
            clickContextOpenLinkInNewTab()
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
        }.openTabDrawer {
        }.openTabsListThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(true, queryString, 3)
        }.openRecentlyVisitedSearchGroupHistoryList(queryString) {
            clickDeleteAllHistoryButton()
            confirmDeleteAllHistory()
            verifyDeleteSnackbarText("Group deleted")
            verifyHistoryItemExists(false, firstPageUrl.toString())
        }.goBack {}
        homeScreen {
            verifyRecentlyVisitedSearchGroupDisplayed(false, queryString, 3)
        }.openThreeDotMenu {
        }.openHistory {
            verifySearchGroupDisplayed(false, queryString, 3)
            verifyEmptyHistoryView()
        }
    }

    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @Test
    fun reopenTabsFromSearchGroupTest() {
        val firstPageUrl = getGenericAsset(searchMockServer, 1).url
        val secondPageUrl = getGenericAsset(searchMockServer, 2).url
        queryString = "test search"
        // setting our custom mockWebServer search URL
        val searchString =
            "http://localhost:${searchMockServer.port}/pages/searchResults.html?search={searchTerms}"
        val customSearchEngine = createSearchEngine(
            name = "TestSearchEngine",
            url = searchString,
            icon = DefaultIconGenerator().generate(appContext, IconRequest(searchString)).bitmap,
        )
        setCustomSearchEngine(customSearchEngine)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickLink("Link 1")
            clickContextOpenLinkInNewTab()
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            mDevice.pressBack()
            longClickLink("Link 2")
            clickContextOpenLinkInNewTab()
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
        }.openTabDrawer {
        }.openTabsListThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(true, queryString, 3)
        }.openRecentlyVisitedSearchGroupHistoryList(queryString) {
        }.openWebsite(firstPageUrl) {
            verifyUrl(firstPageUrl.toString())
        }.goToHomescreen {
        }.openRecentlyVisitedSearchGroupHistoryList(queryString) {
            longTapSelectItem(firstPageUrl)
            longTapSelectItem(secondPageUrl)
            openActionBarOverflowOrOptionsMenu(activityTestRule.activity)
        }

        multipleSelectionToolbar {
        }.clickOpenNewTab {
            verifyNormalModeSelected()
        }.closeTabDrawer {}
        openActionBarOverflowOrOptionsMenu(activityTestRule.activity)
        multipleSelectionToolbar {
        }.clickOpenPrivateTab {
            verifyPrivateModeSelected()
        }
    }

    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @Test
    fun sharePageFromASearchGroupTest() {
        val firstPageUrl = getGenericAsset(searchMockServer, 1).url
        queryString = "test search"
        // setting our custom mockWebServer search URL
        val searchString =
            "http://localhost:${searchMockServer.port}/pages/searchResults.html?search={searchTerms}"
        val customSearchEngine = createSearchEngine(
            name = "TestSearchEngine",
            url = searchString,
            icon = DefaultIconGenerator().generate(appContext, IconRequest(searchString)).bitmap,
        )
        setCustomSearchEngine(customSearchEngine)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickLink("Link 1")
            clickContextOpenLinkInNewTab()
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            mDevice.pressBack()
            longClickLink("Link 2")
            clickContextOpenLinkInNewTab()
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
        }.openTabDrawer {
        }.openTabsListThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(true, queryString, 3)
        }.openRecentlyVisitedSearchGroupHistoryList(queryString) {
            longTapSelectItem(firstPageUrl)
        }

        multipleSelectionToolbar {
            clickShareHistoryButton()
            verifyShareOverlay()
            verifyShareTabFavicon()
            verifyShareTabTitle()
            verifyShareTabUrl()
        }
    }

    // Default search code for Google-US
    @Test
    fun defaultSearchCodeGoogleUS() {
        queryString = "firefox"

        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            verifyUrl(searchEngineCodes["Google"]!!)
        }
    }

    // Default search code for Bing-US
    @Test
    fun defaultSearchCodeBingUS() {
        queryString = "firefox"

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            changeDefaultSearchEngine("Bing")
        }

        exitMenu()

        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            verifyUrl(searchEngineCodes["Bing"]!!)
        }
    }

    // Default search code for DuckDuckGo-US
    @Test
    fun defaultSearchCodeDuckDuckGoUS() {
        queryString = "firefox"

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            changeDefaultSearchEngine("DuckDuckGo")
        }

        exitMenu()

        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            verifyUrl(searchEngineCodes["DuckDuckGo"]!!)
        }
    }
}
