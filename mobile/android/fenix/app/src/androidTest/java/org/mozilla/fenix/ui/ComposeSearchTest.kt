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
import org.mozilla.fenix.helpers.AppAndSystemHelper.assertNativeAppOpens
import org.mozilla.fenix.helpers.AppAndSystemHelper.denyPermission
import org.mozilla.fenix.helpers.AppAndSystemHelper.grantSystemPermission
import org.mozilla.fenix.helpers.AppAndSystemHelper.verifyKeyboardVisibility
import org.mozilla.fenix.helpers.Constants
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.MatcherHelper
import org.mozilla.fenix.helpers.MockBrowserDataHelper.createBookmarkItem
import org.mozilla.fenix.helpers.MockBrowserDataHelper.createTabItem
import org.mozilla.fenix.helpers.MockBrowserDataHelper.setCustomSearchEngine
import org.mozilla.fenix.helpers.SearchDispatcher
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.helpers.TestHelper.verifySnackBarText
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1059459
    @SmokeTest
    @Test
    fun verifyQRScanningCameraAccessDialogTest() {
        val cameraManager = TestHelper.appContext.getSystemService(Context.CAMERA_SERVICE) as CameraManager
        Assume.assumeTrue(cameraManager.cameraIdList.isNotEmpty())

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
            assertNativeAppOpens(Constants.PackageName.ANDROID_SETTINGS)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/235397
    @SmokeTest
    @Test
    fun scanQRCodeToOpenAWebpageTest() {
        val cameraManager = TestHelper.appContext.getSystemService(Context.CAMERA_SERVICE) as CameraManager
        Assume.assumeTrue(cameraManager.cameraIdList.isNotEmpty())

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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/235395
    // Verifies a temporary change of search engine from the Search shortcut menu
    @SmokeTest
    @Test
    fun searchEnginesCanBeChangedTemporarilyFromSearchSelectorMenuTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/233589
    @Test
    fun defaultSearchEnginesCanBeSetFromSearchSelectorMenuTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/522918
    @Test
    fun verifyClearSearchButtonTest() {
        homeScreen {
        }.openSearch {
            typeSearch(queryString)
            clickClearButton()
            verifySearchBarPlaceholder("Search or enter address")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1623441
    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @SmokeTest
    @Test
    fun searchResultsOpenedInNewTabsGenerateSearchGroupsTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1592229
    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @Test
    fun verifyAPageIsAddedToASearchGroupOnlyOnceTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1591782
    @Ignore("Failing due to known bug, see https://github.com/mozilla-mobile/fenix/issues/23818")
    @Test
    fun searchGroupIsGeneratedWhenNavigatingInTheSameTabTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1591781
    @SmokeTest
    @Test
    fun searchGroupIsNotGeneratedForLinksOpenedInPrivateTabsTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1592269
    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @SmokeTest
    @Test
    fun deleteIndividualHistoryItemsFromSearchGroupTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1592242
    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @Test
    fun deleteSearchGroupFromHomeScreenTest() {
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
            verifySnackBarText(expectedText = "Group deleted")
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1592235
    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @Test
    fun openAPageFromHomeScreenSearchGroupTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1592238
    @Ignore("Test run timing out: https://github.com/mozilla-mobile/fenix/issues/27704")
    @Test
    fun shareAPageFromHomeScreenSearchGroupTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1232633
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
                verifyHistoryItemExists(shouldExist = true, Constants.searchEngineCodes["Google"]!!)
            } catch (e: AssertionError) {
                openSearchGroup(queryString)
                verifyHistoryItemExists(shouldExist = true, Constants.searchEngineCodes["Google"]!!)
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1232637
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
            // A search group is sometimes created when searching with Bing (probably redirects)
            try {
                verifyHistoryItemExists(shouldExist = true, Constants.searchEngineCodes["Bing"]!!)
            } catch (e: AssertionError) {
                openSearchGroup(queryString)
                verifyHistoryItemExists(shouldExist = true, Constants.searchEngineCodes["Bing"]!!)
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1232638
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1850517
    // Test that verifies the Firefox Suggest results in a general search context
    @Test
    fun verifyFirefoxSuggestHeaderForBrowsingDataSuggestionsTest() {
        val firstPage = TestAssetHelper.getGenericAsset(searchMockServer, 1)
        val secondPage = TestAssetHelper.getGenericAsset(searchMockServer, 2)

        createTabItem(firstPage.url.toString())
        createBookmarkItem(secondPage.url.toString(), secondPage.title, 1u)

        homeScreen {
        }.openSearch {
            typeSearch("generic")
            verifySearchEngineSuggestionResults(
                rule = activityTestRule,
                searchSuggestions = arrayOf(
                    "Firefox Suggest",
                    firstPage.url.toString(),
                    secondPage.url.toString(),
                ),
                searchTerm = "generic",
            )
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2154197
    @Test
    fun verifyTabsSearchItemsTest() {
        navigationToolbar {
        }.clickUrlbar {
            clickSearchSelectorButton()
            selectTemporarySearchMethod("Tabs")
            verifyKeyboardVisibility(isExpectedToBeVisible = true)
            verifyScanButtonVisibility(visible = false)
            verifyVoiceSearchButtonVisibility(enabled = true)
            verifySearchBarPlaceholder(text = "Search tabs")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2154198
    @Test
    fun verifyTabsSearchWithoutOpenTabsTest() {
        navigationToolbar {
        }.clickUrlbar {
            clickSearchSelectorButton()
            selectTemporarySearchMethod(searchEngineName = "Tabs")
            typeSearch(searchTerm = "Mozilla")
            verifySuggestionsAreNotDisplayed(rule = activityTestRule, "Mozilla")
            clickClearButton()
            verifySearchBarPlaceholder("Search tabs")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2154199
    @SmokeTest
    @Test
    fun verifyTabsSearchWithOpenTabsTest() {
        val firstPageUrl = TestAssetHelper.getGenericAsset(searchMockServer, 1)
        val secondPageUrl = TestAssetHelper.getGenericAsset(searchMockServer, 2)

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
        }.openComposeTabDrawer(activityTestRule) {
            verifyOpenTabsOrder(position = 1, title = firstPageUrl.url.toString())
            verifyOpenTabsOrder(position = 2, title = secondPageUrl.url.toString())
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2154203
    @Test
    fun verifyBookmarksSearchItemsTest() {
        navigationToolbar {
        }.clickSearchSelectorButton {
            selectTemporarySearchMethod("Bookmarks")
            verifySearchBarPlaceholder("Search bookmarks")
            verifyKeyboardVisibility(isExpectedToBeVisible = true)
            verifyScanButtonVisibility(visible = false)
            verifyVoiceSearchButtonVisibility(enabled = true)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2154204
    @Test
    fun verifyBookmarkSearchWithNoBookmarksTest() {
        navigationToolbar {
        }.clickSearchSelectorButton {
            selectTemporarySearchMethod("Bookmarks")
            typeSearch("test")
            verifySuggestionsAreNotDisplayed(activityTestRule, "test")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2154206
    @Test
    fun verifyBookmarksSearchForBookmarkedItemsTest() {
        createBookmarkItem(url = "https://bookmarktest1.com", title = "Test1", position = 1u)
        createBookmarkItem(url = "https://bookmarktest2.com", title = "Test2", position = 2u)

        navigationToolbar {
        }.clickSearchSelectorButton {
            selectTemporarySearchMethod("Bookmarks")
            typeSearch("test")
            verifySearchEngineSuggestionResults(
                activityTestRule,
                searchSuggestions = arrayOf(
                    "Firefox Suggest",
                    "Test1",
                    "https://bookmarktest1.com/",
                    "Test2",
                    "https://bookmarktest2.com/",
                ),
                searchTerm = "test",
            )
        }.dismissSearchBar {
        }.openSearch {
            typeSearch("mozilla ")
            verifySuggestionsAreNotDisplayed(activityTestRule, "Test1", "Test2")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2154212
    @Test
    fun verifyHistorySearchItemsTest() {
        navigationToolbar {
        }.clickUrlbar {
            clickSearchSelectorButton()
            selectTemporarySearchMethod("History")
            verifyKeyboardVisibility(isExpectedToBeVisible = true)
            verifyScanButtonVisibility(visible = false)
            verifyVoiceSearchButtonVisibility(enabled = true)
            verifySearchBarPlaceholder(text = "Search history")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2154213
    @Test
    fun verifyHistorySearchWithoutBrowsingHistoryTest() {
        navigationToolbar {
        }.clickUrlbar {
            clickSearchSelectorButton()
            selectTemporarySearchMethod(searchEngineName = "History")
            typeSearch(searchTerm = "Mozilla")
            verifySuggestionsAreNotDisplayed(rule = activityTestRule, "Mozilla")
            clickClearButton()
            verifySearchBarPlaceholder("Search history")
        }
    }
}
