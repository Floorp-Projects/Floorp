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
import mozilla.components.concept.engine.utils.EngineReleaseChannel
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.Constants.PackageName.ANDROID_SETTINGS
import org.mozilla.fenix.helpers.Constants.searchEngineCodes
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.MockBrowserDataHelper.createBookmarkItem
import org.mozilla.fenix.helpers.MockBrowserDataHelper.createHistoryItem
import org.mozilla.fenix.helpers.MockBrowserDataHelper.createTabItem
import org.mozilla.fenix.helpers.MockBrowserDataHelper.setCustomSearchEngine
import org.mozilla.fenix.helpers.SearchDispatcher
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestHelper.appContext
import org.mozilla.fenix.helpers.TestHelper.assertNativeAppOpens
import org.mozilla.fenix.helpers.TestHelper.clickSnackbarButton
import org.mozilla.fenix.helpers.TestHelper.denyPermission
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.helpers.TestHelper.grantSystemPermission
import org.mozilla.fenix.helpers.TestHelper.longTapSelectItem
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.runWithCondition
import org.mozilla.fenix.helpers.TestHelper.verifyKeyboardVisibility
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

class SearchTest {
    private lateinit var searchMockServer: MockWebServer
    private var queryString = "firefox"
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
            isCookieBannerReductionDialogEnabled = false,
            tabsTrayRewriteEnabled = false,
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2154189
    @Test
    fun verifySearchBarItemsTest() {
        navigationToolbar {
            verifyDefaultSearchEngine("Google")
            verifySearchBarPlaceholder("Search or enter address")
        }.clickUrlbar {
            verifyKeyboardVisibility(isExpectedToBeVisible = true)
            verifyScanButtonVisibility(visible = true)
            verifyVoiceSearchButtonVisibility(enabled = true)
            verifySearchBarPlaceholder("Search or enter address")
            typeSearch("mozilla ")
            verifyScanButtonVisibility(visible = false)
            verifyVoiceSearchButtonVisibility(enabled = true)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2154190
    @Test
    fun verifySearchSelectorMenuItemsTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2154194
    @Test
    fun verifySearchPlaceholderForGeneralDefaultSearchEnginesTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2154195
    @Test
    fun verifySearchPlaceholderForNotDefaultGeneralSearchEnginesTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2154196
    @Test
    fun verifySearchPlaceholderForTopicSpecificSearchEnginesTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2154191
    @Test
    fun verifyScanButtonAvailableOnlyForGeneralSearchEnginesTest() {
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
            exitMenu()
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
        // setting our custom mockWebServer search URL
        val searchEngineName = "TestSearchEngine"
        setCustomSearchEngine(searchMockServer, searchEngineName)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickPageObject(itemWithText("Link 1"))
            clickContextMenuItem("Open link in new tab")
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            pressBack()
            longClickPageObject(itemWithText("Link 2"))
            clickContextMenuItem("Open link in new tab")
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
        }.openTabDrawer {
        }.openTabsListThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(shouldBeDisplayed = true, searchTerm = queryString, groupSize = 3)
        }
    }

    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @Test
    fun verifySearchGroupHistoryWithNoDuplicatesTest() {
        val firstPageUrl = getGenericAsset(searchMockServer, 1).url
        val secondPageUrl = getGenericAsset(searchMockServer, 2).url
        val originPageUrl =
            "http://localhost:${searchMockServer.port}/pages/searchResults.html?search=test%20search".toUri()
        // setting our custom mockWebServer search URL
        val searchEngineName = "TestSearchEngine"
        setCustomSearchEngine(searchMockServer, searchEngineName)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickPageObject(itemWithText("Link 1"))
            clickContextMenuItem("Open link in new tab")
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            pressBack()
            longClickPageObject(itemWithText("Link 1"))
            clickContextMenuItem("Open link in new tab")
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            pressBack()
            longClickPageObject(itemWithText("Link 2"))
            clickContextMenuItem("Open link in new tab")
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            pressBack()
            longClickPageObject(itemWithText("Link 1"))
            clickContextMenuItem("Open link in new tab")
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
        }.openTabDrawer {
        }.openTabsListThreeDotMenu {
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
            clickPageObject(itemContainingText("Link 1"))
            waitForPageToLoad()
            pressBack()
            clickPageObject(itemContainingText("Link 2"))
            waitForPageToLoad()
        }.openTabDrawer {
        }.openTabsListThreeDotMenu {
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
            longClickPageObject(itemWithText("Link 1"))
            clickContextMenuItem("Open link in private tab")
            longClickPageObject(itemWithText("Link 2"))
            clickContextMenuItem("Open link in private tab")
        }.openTabDrawer {
        }.toggleToPrivateTabs {
        }.openTabWithIndex(0) {
        }.openTabDrawer {
        }.openTabWithIndex(1) {
        }.openTabDrawer {
        }.openTabsListThreeDotMenu {
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
        val firstPageUrl = getGenericAsset(searchMockServer, 1).url
        val secondPageUrl = getGenericAsset(searchMockServer, 2).url
        // setting our custom mockWebServer search URL
        val searchEngineName = "TestSearchEngine"
        setCustomSearchEngine(searchMockServer, searchEngineName)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickPageObject(itemWithText("Link 1"))
            clickContextMenuItem("Open link in new tab")
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            mDevice.pressBack()
            longClickPageObject(itemWithText("Link 2"))
            clickContextMenuItem("Open link in new tab")
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
        }.openTabDrawer {
        }.openTabsListThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(shouldBeDisplayed = true, searchTerm = queryString, groupSize = 3)
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
            verifyRecentlyVisitedSearchGroupDisplayed(shouldBeDisplayed = false, searchTerm = queryString, groupSize = 1)
        }
    }

    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @Test
    fun deleteSearchGroupFromHistoryTest() {
        val firstPageUrl = getGenericAsset(searchMockServer, 1).url
        // setting our custom mockWebServer search URL
        val searchEngineName = "TestSearchEngine"
        setCustomSearchEngine(searchMockServer, searchEngineName)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickPageObject(itemWithText("Link 1"))
            clickContextMenuItem("Open link in new tab")
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            mDevice.pressBack()
            longClickPageObject(itemWithText("Link 2"))
            clickContextMenuItem("Open link in new tab")
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
        }.openTabDrawer {
        }.openTabsListThreeDotMenu {
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
        val firstPageUrl = getGenericAsset(searchMockServer, 1).url
        val secondPageUrl = getGenericAsset(searchMockServer, 2).url
        // setting our custom mockWebServer search URL
        val searchEngineName = "TestSearchEngine"
        setCustomSearchEngine(searchMockServer, searchEngineName)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickPageObject(itemWithText("Link 1"))
            clickContextMenuItem("Open link in new tab")
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            mDevice.pressBack()
            longClickPageObject(itemWithText("Link 2"))
            clickContextMenuItem("Open link in new tab")
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
        }.openTabDrawer {
        }.openTabsListThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(shouldBeDisplayed = true, searchTerm = queryString, groupSize = 3)
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
        // setting our custom mockWebServer search URL
        val searchEngineName = "TestSearchEngine"
        setCustomSearchEngine(searchMockServer, searchEngineName)

        // Performs a search and opens 2 dummy search results links to create a search group
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            longClickPageObject(itemWithText("Link 1"))
            clickContextMenuItem("Open link in new tab")
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
            mDevice.pressBack()
            longClickPageObject(itemWithText("Link 2"))
            clickContextMenuItem("Open link in new tab")
            clickSnackbarButton("SWITCH")
            waitForPageToLoad()
        }.openTabDrawer {
        }.openTabsListThreeDotMenu {
        }.closeAllTabs {
            verifyRecentlyVisitedSearchGroupDisplayed(shouldBeDisplayed = true, searchTerm = queryString, groupSize = 3)
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
        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            waitForPageToLoad()
        }.openThreeDotMenu {
        }.openHistory {
            // Full URL no longer visible in the nav bar, so we'll check the history record
            // A search group is sometimes created when searching with Google (probably redirects)
            try {
                verifyHistoryItemExists(shouldExist = true, searchEngineCodes["Google"]!!)
            } catch (e: AssertionError) {
                openSearchGroup(queryString)
                verifyHistoryItemExists(shouldExist = true, searchEngineCodes["Google"]!!)
            }
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
            exitMenu()
        }

        homeScreen {
        }.openSearch {
        }.submitQuery(queryString) {
            waitForPageToLoad()
        }.openThreeDotMenu {
        }.openHistory {
            // Full URL no longer visible in the nav bar, so we'll check the history record
            verifyHistoryItemExists(shouldExist = true, searchEngineCodes["Bing"]!!)
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
            exitMenu()
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
                verifyHistoryItemExists(shouldExist = true, item = searchEngineCodes["DuckDuckGo"]!!)
            } catch (e: AssertionError) {
                openSearchGroup(queryString)
                verifyHistoryItemExists(shouldExist = true, item = searchEngineCodes["DuckDuckGo"]!!)
            }
        }
    }

    @SmokeTest
    @Test
    fun verifySearchHistoryWithBrowsingDataTest() {
        val firstPageUrl = getGenericAsset(searchMockServer, 1)
        val secondPageUrl = getGenericAsset(searchMockServer, 2)

        createHistoryItem(firstPageUrl.url.toString())
        createHistoryItem(secondPageUrl.url.toString())

        navigationToolbar {
        }.clickUrlbar {
            clickSearchSelectorButton()
            selectTemporarySearchMethod(searchEngineName = "History")
            typeSearch(searchTerm = "Mozilla")
            verifySuggestionsAreNotDisplayed(rule = activityTestRule, "Mozilla")
            clickClearButton()
            typeSearch(searchTerm = "generic")
            verifyTypedToolbarText("generic")
            verifySearchEngineSuggestionResults(
                rule = activityTestRule,
                searchSuggestions = arrayOf(
                    firstPageUrl.url.toString(),
                    secondPageUrl.url.toString(),
                ),
                searchTerm = "generic",
            )
        }.clickSearchSuggestion(firstPageUrl.url.toString()) {
            verifyUrl(firstPageUrl.url.toString())
        }
    }

    @SmokeTest
    @Test
    fun verifySearchTabsWithOpenTabsTest() {
        runWithCondition(
            // This test will run only on Beta and RC builds
            // The new composable tabs tray is only available in Nightly and Debug.
            activityTestRule.activity.components.core.engine.version.releaseChannel == EngineReleaseChannel.RELEASE ||
                activityTestRule.activity.components.core.engine.version.releaseChannel == EngineReleaseChannel.BETA,
        ) {
            val firstPageUrl = getGenericAsset(searchMockServer, 1)
            val secondPageUrl = getGenericAsset(searchMockServer, 2)

            createTabItem(firstPageUrl.url.toString())
            createTabItem(secondPageUrl.url.toString())

            navigationToolbar {
            }.clickUrlbar {
                clickSearchSelectorButton()
                selectTemporarySearchMethod(searchEngineName = "Tabs")
                typeSearch(searchTerm = "Mozilla")
                verifySuggestionsAreNotDisplayed(rule = activityTestRule, "Mozilla")
                clickClearButton()
                typeSearch(searchTerm = "generic")
                verifyTypedToolbarText("generic")
                verifySearchEngineSuggestionResults(
                    rule = activityTestRule,
                    searchSuggestions = arrayOf(
                        "Firefox Suggest",
                        firstPageUrl.url.toString(),
                        secondPageUrl.url.toString(),
                    ),
                    searchTerm = "generic",
                )
            }.clickSearchSuggestion(firstPageUrl.url.toString()) {
                verifyTabCounter("2")
            }.openTabDrawer {
                verifyOpenTabsOrder(position = 1, title = firstPageUrl.url.toString())
                verifyOpenTabsOrder(position = 2, title = secondPageUrl.url.toString())
            }
        }
    }

    @SmokeTest
    @Test
    fun searchHistoryNotRememberedInPrivateBrowsingTest() {
        appContext.settings().shouldShowSearchSuggestionsInPrivate = true

        val firstPageUrl = getGenericAsset(searchMockServer, 1)
        val searchEngineName = "TestSearchEngine"

        setCustomSearchEngine(searchMockServer, searchEngineName)
        createBookmarkItem(firstPageUrl.url.toString(), firstPageUrl.title, 1u)

        homeScreen {
        }.openNavigationToolbar {
        }.clickUrlbar {
        }.submitQuery("test page 1") {
        }.goToHomescreen {
        }.togglePrivateBrowsingMode()

        homeScreen {
        }.openNavigationToolbar {
        }.clickUrlbar {
        }.submitQuery("test page 2") {
        }.openNavigationToolbar {
        }.clickUrlbar {
            typeSearch(searchTerm = "test page")
            verifySearchEngineSuggestionResults(
                rule = activityTestRule,
                searchSuggestions = arrayOf(
                    "TestSearchEngine search",
                    "test page 1",
                    "Firefox Suggest",
                    firstPageUrl.url.toString(),
                ),
                searchTerm = "test page 1",
            )
            // 2 search engine suggestions and 2 browser suggestions (1 history, 1 bookmark)
            verifySearchSuggestionsCount(activityTestRule, numberOfSuggestions = 4, searchTerm = "test page")
            verifySuggestionsAreNotDisplayed(
                activityTestRule,
                searchSuggestions = arrayOf(
                    "test page 2",
                ),
            )
        }
    }
}
