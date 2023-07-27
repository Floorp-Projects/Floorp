/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.content.Context
import android.hardware.camera2.CameraManager
import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.core.net.toUri
import androidx.test.espresso.Espresso
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Assume
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.Constants
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.MatcherHelper
import org.mozilla.fenix.helpers.MockBrowserDataHelper.setCustomSearchEngine
import org.mozilla.fenix.helpers.SearchDispatcher
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.ui.robots.clickContextMenuItem
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.longClickPageObject
import org.mozilla.fenix.ui.robots.multipleSelectionToolbar
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.searchScreen

/**
 *  Tests for verifying the search fragment
 *
 *  Including:
 * - Verify the toolbar, awesomebar, and shortcut bar are displayed
 * - Select shortcut button
 * - Select scan button
 *
 */

class ComposeSearchTest {
    lateinit var searchMockServer: MockWebServer
    private val queryString: String = "firefox"
    private val generalEnginesList = listOf("DuckDuckGo", "Google", "Bing")
    private val topicEnginesList = listOf("Amazon.com", "Wikipedia", "eBay")

    @get:Rule
    val activityTestRule = AndroidComposeTestRule(
        HomeActivityTestRule(
            skipOnboarding = true,
            isPocketEnabled = false,
            isJumpBackInCFREnabled = false,
            isRecentTabsFeatureEnabled = false,
            isTCPCFREnabled = false,
            isWallpaperOnboardingEnabled = false,
            tabsTrayRewriteEnabled = true,
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
    fun searchBarItemsTest() {
        navigationToolbar {
            verifyDefaultSearchEngine("Google")
            verifySearchBarPlaceholder("Search or enter address")
        }.clickUrlbar {
            TestHelper.verifyKeyboardVisibility(isExpectedToBeVisible = true)
            verifyScanButtonVisibility(visible = true)
            verifyVoiceSearchButtonVisibility(enabled = true)
            verifySearchBarPlaceholder("Search or enter address")
            typeSearch("mozilla ")
            verifyScanButtonVisibility(visible = false)
            verifyVoiceSearchButtonVisibility(enabled = true)
        }
    }

    @Test
    fun searchSelectorMenuItemsTest() {
        homeScreen {
        }.openSearch {
            verifySearchView()
            verifySearchToolbar(isDisplayed = true)
            clickSearchSelectorButton()
            verifySearchShortcutListContains(
                "DuckDuckGo", "Google", "Amazon.com", "Wikipedia", "Bing", "eBay",
                "Bookmarks", "Tabs", "History", "Search settings",
            )
        }
    }

    @Test
    fun searchPlaceholderForDefaultEnginesTest() {
        generalEnginesList.forEach {
            homeScreen {
            }.openSearch {
                clickSearchSelectorButton()
            }.clickSearchEngineSettings {
                openDefaultSearchEngineMenu()
                changeDefaultSearchEngine(it)
                exitMenu()
            }
            navigationToolbar {
                verifySearchBarPlaceholder("Search or enter address")
            }
        }
    }

    @Test
    fun searchPlaceholderForOtherGeneralSearchEnginesTest() {
        val generalEnginesList = listOf("DuckDuckGo", "Bing")

        generalEnginesList.forEach {
            homeScreen {
            }.openSearch {
                clickSearchSelectorButton()
                selectTemporarySearchMethod(it)
                verifySearchBarPlaceholder("Search the web")
            }.dismissSearchBar {}
        }
    }

    @Test
    fun searchPlaceholderForTopicSearchEngineTest() {
        val topicEnginesList = listOf("Amazon.com", "Wikipedia", "eBay")

        topicEnginesList.forEach {
            homeScreen {
            }.openSearch {
                clickSearchSelectorButton()
                selectTemporarySearchMethod(it)
                verifySearchBarPlaceholder("Enter search terms")
            }.dismissSearchBar {}
        }
    }

    @SmokeTest
    @Test
    fun scanButtonDenyPermissionTest() {
        val cameraManager = TestHelper.appContext.getSystemService(Context.CAMERA_SERVICE) as CameraManager
        Assume.assumeTrue(cameraManager.cameraIdList.isNotEmpty())

        homeScreen {
        }.openSearch {
            clickScanButton()
            TestHelper.denyPermission()
            clickScanButton()
            clickDismissPermissionRequiredDialog()
        }
        homeScreen {
        }.openSearch {
            clickScanButton()
            clickGoToPermissionsSettings()
            TestHelper.assertNativeAppOpens(Constants.PackageName.ANDROID_SETTINGS)
        }
    }

    @SmokeTest
    @Test
    fun scanButtonAllowPermissionTest() {
        val cameraManager = TestHelper.appContext.getSystemService(Context.CAMERA_SERVICE) as CameraManager
        Assume.assumeTrue(cameraManager.cameraIdList.isNotEmpty())

        homeScreen {
        }.openSearch {
            clickScanButton()
            TestHelper.grantSystemPermission()
            verifyScannerOpen()
        }
    }

    @Test
    fun scanButtonAvailableOnlyForGeneralSearchEnginesTest() {
        generalEnginesList.forEach {
            homeScreen {
            }.openSearch {
                clickSearchSelectorButton()
                selectTemporarySearchMethod(it)
                verifyScanButtonVisibility(visible = true)
            }.dismissSearchBar {}
        }

        topicEnginesList.forEach {
            homeScreen {
            }.openSearch {
                clickSearchSelectorButton()
                selectTemporarySearchMethod(it)
                verifyScanButtonVisibility(visible = false)
            }.dismissSearchBar {}
        }
    }

    // Verifies a temporary change of search engine from the Search shortcut menu
    @SmokeTest
    @Test
    fun selectSearchEnginesShortcutTest() {
        val enginesList = listOf("DuckDuckGo", "Google", "Amazon.com", "Wikipedia", "Bing", "eBay")

        enginesList.forEach {
            homeScreen {
            }.openSearch {
                clickSearchSelectorButton()
                verifySearchShortcutListContains(it)
                selectTemporarySearchMethod(it)
                verifySearchEngineIcon(it)
            }.submitQuery("mozilla ") {
                verifyUrl("mozilla")
            }.goToHomescreen {}
        }
    }

    @Test
    fun accessSearchSettingFromSearchSelectorMenuTest() {
        searchScreen {
            clickSearchSelectorButton()
        }.clickSearchEngineSettings {
            verifyToolbarText("Search")
            openDefaultSearchEngineMenu()
            changeDefaultSearchEngine("DuckDuckGo")
            TestHelper.exitMenu()
        }
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            verifyUrl(queryString)
        }
    }

    @Test
    fun clearSearchTest() {
        homeScreen {
        }.openSearch {
            typeSearch(queryString)
            clickClearButton()
            verifySearchBarPlaceholder("Search or enter address")
        }
    }

    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @SmokeTest
    @Test
    fun searchGroupShowsInRecentlyVisitedTest() {
        val searchEngineName = "TestSearchEngine"
        // setting our custom mockWebServer search URL
        setCustomSearchEngine(searchMockServer, searchEngineName)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickPageObject(MatcherHelper.itemWithText("Link 1"))
            clickContextMenuItem("Open link in new tab")
            TestHelper.clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            Espresso.pressBack()
            longClickPageObject(MatcherHelper.itemWithText("Link 2"))
            clickContextMenuItem("Open link in new tab")
            TestHelper.clickSnackbarButton("SWITCH")
            waitForPageToLoad()
        }.openComposeTabDrawer(activityTestRule) {
        }.openThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(shouldBeDisplayed = true, searchTerm = queryString, groupSize = 3)
        }
    }

    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @Test
    fun verifySearchGroupHistoryWithNoDuplicatesTest() {
        val firstPageUrl = TestAssetHelper.getGenericAsset(searchMockServer, 1).url
        val secondPageUrl = TestAssetHelper.getGenericAsset(searchMockServer, 2).url
        val originPageUrl =
            "http://localhost:${searchMockServer.port}/pages/searchResults.html?search=test%20search".toUri()
        val searchEngineName = "TestSearchEngine"
        // setting our custom mockWebServer search URL
        setCustomSearchEngine(searchMockServer, searchEngineName)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickPageObject(MatcherHelper.itemWithText("Link 1"))
            clickContextMenuItem("Open link in new tab")
            TestHelper.clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            Espresso.pressBack()
            longClickPageObject(MatcherHelper.itemWithText("Link 1"))
            clickContextMenuItem("Open link in new tab")
            TestHelper.clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            Espresso.pressBack()
            longClickPageObject(MatcherHelper.itemWithText("Link 2"))
            clickContextMenuItem("Open link in new tab")
            TestHelper.clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            Espresso.pressBack()
            longClickPageObject(MatcherHelper.itemWithText("Link 1"))
            clickContextMenuItem("Open link in new tab")
            TestHelper.clickSnackbarButton("SWITCH")
            waitForPageToLoad()
        }.openComposeTabDrawer(activityTestRule) {
        }.openThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(shouldBeDisplayed = true, searchTerm = queryString, groupSize = 3)
        }.openRecentlyVisitedSearchGroupHistoryList(queryString) {
            verifyTestPageUrl(firstPageUrl)
            verifyTestPageUrl(secondPageUrl)
            verifyTestPageUrl(originPageUrl)
        }
    }

    @Ignore("Failing due to known bug, see https://github.com/mozilla-mobile/fenix/issues/23818")
    @Test
    fun searchGroupGeneratedInTheSameTabTest() {
        // setting our custom mockWebServer search URL
        val searchEngineName = "TestSearchEngine"
        setCustomSearchEngine(searchMockServer, searchEngineName)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            clickPageObject(MatcherHelper.itemContainingText("Link 1"))
            waitForPageToLoad()
            Espresso.pressBack()
            clickPageObject(MatcherHelper.itemContainingText("Link 2"))
            waitForPageToLoad()
        }.openComposeTabDrawer(activityTestRule) {
        }.openThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(shouldBeDisplayed = true, searchTerm = queryString, groupSize = 3)
        }
    }

    @SmokeTest
    @Test
    fun noSearchGroupFromPrivateBrowsingTest() {
        // setting our custom mockWebServer search URL
        val searchEngineName = "TestSearchEngine"
        setCustomSearchEngine(searchMockServer, searchEngineName)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickPageObject(MatcherHelper.itemWithText("Link 1"))
            clickContextMenuItem("Open link in private tab")
            longClickPageObject(MatcherHelper.itemWithText("Link 2"))
            clickContextMenuItem("Open link in private tab")
        }.openComposeTabDrawer(activityTestRule) {
        }.toggleToPrivateTabs {
        }.openPrivateTab(0) {
        }.openComposeTabDrawer(activityTestRule) {
        }.openPrivateTab(1) {
        }.openComposeTabDrawer(activityTestRule) {
        }.openThreeDotMenu {
        }.closeAllTabs {
            togglePrivateBrowsingModeOnOff()
            verifyRecentlyVisitedSearchGroupDisplayed(shouldBeDisplayed = false, searchTerm = queryString, groupSize = 3)
        }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryItemExists(shouldExist = false, item = "3 sites")
        }
    }

    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @SmokeTest
    @Test
    fun deleteItemsFromSearchGroupHistoryTest() {
        val firstPageUrl = TestAssetHelper.getGenericAsset(searchMockServer, 1).url
        val secondPageUrl = TestAssetHelper.getGenericAsset(searchMockServer, 2).url
        // setting our custom mockWebServer search URL
        val searchEngineName = "TestSearchEngine"
        setCustomSearchEngine(searchMockServer, searchEngineName)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickPageObject(MatcherHelper.itemWithText("Link 1"))
            clickContextMenuItem("Open link in new tab")
            TestHelper.clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            TestHelper.mDevice.pressBack()
            longClickPageObject(MatcherHelper.itemWithText("Link 2"))
            clickContextMenuItem("Open link in new tab")
            TestHelper.clickSnackbarButton("SWITCH")
            waitForPageToLoad()
        }.openComposeTabDrawer(activityTestRule) {
        }.openThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(shouldBeDisplayed = true, searchTerm = queryString, groupSize = 3)
        }.openRecentlyVisitedSearchGroupHistoryList(queryString) {
            clickDeleteHistoryButton(firstPageUrl.toString())
            TestHelper.longTapSelectItem(secondPageUrl)
            multipleSelectionToolbar {
                Espresso.openActionBarOverflowOrOptionsMenu(activityTestRule.activity)
                clickMultiSelectionDelete()
            }
            TestHelper.exitMenu()
        }
        homeScreen {
            // checking that the group is removed when only 1 item is left
            verifyRecentlyVisitedSearchGroupDisplayed(shouldBeDisplayed = false, searchTerm = queryString, groupSize = 1)
        }
    }

    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @Test
    fun deleteSearchGroupFromHistoryTest() {
        val firstPageUrl = TestAssetHelper.getGenericAsset(searchMockServer, 1).url
        // setting our custom mockWebServer search URL
        val searchEngineName = "TestSearchEngine"
        setCustomSearchEngine(searchMockServer, searchEngineName)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickPageObject(MatcherHelper.itemWithText("Link 1"))
            clickContextMenuItem("Open link in new tab")
            TestHelper.clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            TestHelper.mDevice.pressBack()
            longClickPageObject(MatcherHelper.itemWithText("Link 2"))
            clickContextMenuItem("Open link in new tab")
            TestHelper.clickSnackbarButton("SWITCH")
            waitForPageToLoad()
        }.openComposeTabDrawer(activityTestRule) {
        }.openThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(shouldBeDisplayed = true, searchTerm = queryString, groupSize = 3)
        }.openRecentlyVisitedSearchGroupHistoryList(queryString) {
            clickDeleteAllHistoryButton()
            confirmDeleteAllHistory()
            verifyDeleteSnackbarText("Group deleted")
            verifyHistoryItemExists(shouldExist = false, firstPageUrl.toString())
        }.goBack {}
        homeScreen {
            verifyRecentlyVisitedSearchGroupDisplayed(shouldBeDisplayed = false, queryString, groupSize = 3)
        }.openThreeDotMenu {
        }.openHistory {
            verifySearchGroupDisplayed(shouldBeDisplayed = false, queryString, groupSize = 3)
            verifyEmptyHistoryView()
        }
    }

    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @Test
    fun reopenTabsFromSearchGroupTest() {
        val firstPageUrl = TestAssetHelper.getGenericAsset(searchMockServer, 1).url
        val secondPageUrl = TestAssetHelper.getGenericAsset(searchMockServer, 2).url

        // setting our custom mockWebServer search URL
        val searchEngineName = "TestSearchEngine"
        setCustomSearchEngine(searchMockServer, searchEngineName)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickPageObject(MatcherHelper.itemWithText("Link 1"))
            clickContextMenuItem("Open link in new tab")
            TestHelper.clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            TestHelper.mDevice.pressBack()
            longClickPageObject(MatcherHelper.itemWithText("Link 2"))
            clickContextMenuItem("Open link in new tab")
            TestHelper.clickSnackbarButton("SWITCH")
            waitForPageToLoad()
        }.openComposeTabDrawer(activityTestRule) {
        }.openThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(shouldBeDisplayed = true, searchTerm = queryString, groupSize = 3)
        }.openRecentlyVisitedSearchGroupHistoryList(queryString) {
        }.openWebsite(firstPageUrl) {
            verifyUrl(firstPageUrl.toString())
        }.goToHomescreen {
        }.openRecentlyVisitedSearchGroupHistoryList(queryString) {
            TestHelper.longTapSelectItem(firstPageUrl)
            TestHelper.longTapSelectItem(secondPageUrl)
            Espresso.openActionBarOverflowOrOptionsMenu(activityTestRule.activity)
        }

        multipleSelectionToolbar {
        }.clickOpenNewTab(activityTestRule) {
            verifyNormalBrowsingButtonIsSelected()
        }.closeTabDrawer {}
        Espresso.openActionBarOverflowOrOptionsMenu(activityTestRule.activity)
        multipleSelectionToolbar {
        }.clickOpenPrivateTab {
            verifyPrivateModeSelected()
        }
    }

    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @Test
    fun sharePageFromASearchGroupTest() {
        val firstPageUrl = TestAssetHelper.getGenericAsset(searchMockServer, 1).url
        // setting our custom mockWebServer search URL
        val searchEngineName = "TestSearchEngine"
        setCustomSearchEngine(searchMockServer, searchEngineName)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickPageObject(MatcherHelper.itemWithText("Link 1"))
            clickContextMenuItem("Open link in new tab")
            TestHelper.clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            TestHelper.mDevice.pressBack()
            longClickPageObject(MatcherHelper.itemWithText("Link 2"))
            clickContextMenuItem("Open link in new tab")
            TestHelper.clickSnackbarButton("SWITCH")
            waitForPageToLoad()
        }.openComposeTabDrawer(activityTestRule) {
        }.openThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(shouldBeDisplayed = true, searchTerm = queryString, groupSize = 3)
        }.openRecentlyVisitedSearchGroupHistoryList(queryString) {
            TestHelper.longTapSelectItem(firstPageUrl)
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
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            waitForPageToLoad()
        }.openThreeDotMenu {
        }.openHistory {
            // Full URL no longer visible in the nav bar, so we'll check the history record
            verifyHistoryItemExists(shouldExist = true, Constants.searchEngineCodes["Google"]!!)
        }
    }

    // Default search code for Bing-US
    @Test
    fun defaultSearchCodeBingUS() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            openDefaultSearchEngineMenu()
            changeDefaultSearchEngine("Bing")
            TestHelper.exitMenu()
        }

        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            waitForPageToLoad()
        }.openThreeDotMenu {
        }.openHistory {
            // Full URL no longer visible in the nav bar, so we'll check the history record
            verifyHistoryItemExists(shouldExist = true, Constants.searchEngineCodes["Bing"]!!)
        }
    }

    // Default search code for DuckDuckGo-US
    @Test
    fun defaultSearchCodeDuckDuckGoUS() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            openDefaultSearchEngineMenu()
            changeDefaultSearchEngine("DuckDuckGo")
            TestHelper.exitMenu()
        }
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            waitForPageToLoad()
        }.openThreeDotMenu {
        }.openHistory {
            // Full URL no longer visible in the nav bar, so we'll check the history record
            // A search group is sometimes created when searching with DuckDuckGo
            try {
                verifyHistoryItemExists(shouldExist = true, item = Constants.searchEngineCodes["DuckDuckGo"]!!)
            } catch (e: AssertionError) {
                openSearchGroup(queryString)
                verifyHistoryItemExists(shouldExist = true, item = Constants.searchEngineCodes["DuckDuckGo"]!!)
            }
        }
    }
}
