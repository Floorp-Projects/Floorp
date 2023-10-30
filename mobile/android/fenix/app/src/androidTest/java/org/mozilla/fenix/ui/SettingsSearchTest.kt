/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.test.espresso.Espresso.pressBack
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.AppAndSystemHelper.runWithSystemLocaleChanged
import org.mozilla.fenix.helpers.AppAndSystemHelper.setSystemLocale
import org.mozilla.fenix.helpers.DataGenerationHelper.setTextToClipBoard
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MockBrowserDataHelper.addCustomSearchEngine
import org.mozilla.fenix.helpers.MockBrowserDataHelper.createBookmarkItem
import org.mozilla.fenix.helpers.MockBrowserDataHelper.createHistoryItem
import org.mozilla.fenix.helpers.SearchDispatcher
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestHelper.appContext
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.helpers.TestHelper.restartApp
import org.mozilla.fenix.helpers.TestHelper.verifySnackBarText
import org.mozilla.fenix.ui.robots.EngineShortcut
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.searchScreen
import java.util.Locale

class SettingsSearchTest {
    private lateinit var mockWebServer: MockWebServer
    private lateinit var searchMockServer: MockWebServer
    private val defaultSearchEngineList =
        listOf(
            "Bing",
            "DuckDuckGo",
            "Google",
        )

    @get:Rule
    val activityTestRule = AndroidComposeTestRule(
        HomeActivityIntentTestRule.withDefaultSettingsOverrides(),
    ) { it.activity }

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }

        searchMockServer = MockWebServer().apply {
            dispatcher = SearchDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2203333
    @Test
    fun verifySearchSettingsMenuItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            verifyToolbarText("Search")
            verifySearchEnginesSectionHeader()
            verifyDefaultSearchEngineHeader()
            verifyDefaultSearchEngineSummary("Google")
            verifyManageSearchShortcutsHeader()
            verifyManageShortcutsSummary()
            verifyAddressBarSectionHeader()
            verifyAutocompleteURlsIsEnabled(true)
            verifyShowClipboardSuggestionsEnabled(true)
            verifySearchBrowsingHistoryEnabled(true)
            verifySearchBookmarksEnabled(true)
            verifySearchSyncedTabsEnabled(true)
            verifyVoiceSearchEnabled(true)
            verifyShowSearchSuggestionsEnabled(true)
            verifyShowSearchSuggestionsInPrivateEnabled(false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2203307
    @Test
    fun verifyDefaultSearchEnginesSettingsItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            verifyDefaultSearchEngineHeader()
            openDefaultSearchEngineMenu()
            verifyToolbarText("Default search engine")
            verifyDefaultSearchEngineList()
            verifyDefaultSearchEngineSelected("Google")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2203308
    @SmokeTest
    @Test
    fun verifyTheDefaultSearchEngineCanBeChangedTest() {
        // Goes through the settings and changes the default search engine, then verifies it has changed.
        defaultSearchEngineList.forEach {
            homeScreen {
            }.openThreeDotMenu {
            }.openSettings {
            }.openSearchSubMenu {
                openDefaultSearchEngineMenu()
                changeDefaultSearchEngine(it)
                exitMenu()
            }
            searchScreen {
                verifySearchEngineIcon(it)
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/233586
    @Test
    fun verifyUrlAutocompleteToggleTest() {
        homeScreen {
        }.openSearch {
            typeSearch("mo")
            verifyTypedToolbarText("monster.com")
            typeSearch("moz")
            verifyTypedToolbarText("mozilla.org")
        }.dismissSearchBar {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            toggleAutocomplete()
        }.goBack {
        }.goBack {
        }.openSearch {
            typeSearch("moz")
            verifyTypedToolbarText("moz")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/361817
    @Test
    fun disableSearchBrowsingHistorySuggestionsToggleTest() {
        val websiteURL = getGenericAsset(mockWebServer, 1).url.toString()

        createHistoryItem(websiteURL)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            switchSearchHistoryToggle()
            exitMenu()
        }

        homeScreen {
        }.openSearch {
            typeSearch("test")
            verifySuggestionsAreNotDisplayed(
                activityTestRule,
                "Firefox Suggest",
                websiteURL,
            )
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/412926
    @Test
    fun disableSearchBookmarksToggleTest() {
        val website = getGenericAsset(mockWebServer, 1)

        createBookmarkItem(website.url.toString(), website.title, 1u)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            switchSearchBookmarksToggle()
            // We want to avoid confusion between history and bookmarks searches,
            // so we'll disable this too.
            switchSearchHistoryToggle()
            exitMenu()
        }

        homeScreen {
        }.openSearch {
            typeSearch("test")
            verifySuggestionsAreNotDisplayed(
                activityTestRule,
                "Firefox Suggest",
                website.title,
            )
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2203309
    // Verifies setting as default a customized search engine name and URL
    @SmokeTest
    @Test
    fun verifyCustomSearchEngineCanBeAddedFromSearchEngineMenuTest() {
        val customSearchEngine = object {
            val title = "TestSearchEngine"
            val url = "http://localhost:${searchMockServer.port}/searchResults.html?search=%s"
        }

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            openDefaultSearchEngineMenu()
            openAddSearchEngineMenu()
            verifySaveSearchEngineButtonEnabled(false)
            typeCustomEngineDetails(customSearchEngine.title, customSearchEngine.url)
            verifySaveSearchEngineButtonEnabled(true)
            saveNewSearchEngine()
            verifySnackBarText("Created ${customSearchEngine.title}")
            verifyEngineListContains(customSearchEngine.title, shouldExist = true)
            openEngineOverflowMenu(customSearchEngine.title)
            pressBack()
            changeDefaultSearchEngine(customSearchEngine.title)
            pressBack()
            openManageShortcutsMenu()
            verifyEngineListContains(customSearchEngine.title, shouldExist = true)
            pressBack()
        }.goBack {
            verifySettingsOptionSummary("Search", customSearchEngine.title)
        }.goBack {
        }.openSearch {
            verifySearchEngineIcon(customSearchEngine.title)
            clickSearchSelectorButton()
            verifySearchShortcutListContains(customSearchEngine.title)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2203335
    @Test
    fun addCustomSearchEngineToManageShortcutsListTest() {
        val customSearchEngine = object {
            val title = "TestSearchEngine"
            val url = "http://localhost:${searchMockServer.port}/searchResults.html?search=%s"
        }

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            openManageShortcutsMenu()
            openAddSearchEngineMenu()
            typeCustomEngineDetails(customSearchEngine.title, customSearchEngine.url)
            saveNewSearchEngine()
            verifyEngineListContains(customSearchEngine.title, shouldExist = true)
            pressBack()
            openDefaultSearchEngineMenu()
            verifyEngineListContains(customSearchEngine.title, shouldExist = true)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2203343
    @Test
    fun verifyLearnMoreLinksFromAddSearchEngineSectionTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            openDefaultSearchEngineMenu()
            openAddSearchEngineMenu()
        }.clickCustomSearchStringLearnMoreLink {
            verifyUrl(
                "support.mozilla.org/en-US/kb/manage-my-default-search-engines-firefox-android?as=u&utm_source=inproduct",
            )
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            openDefaultSearchEngineMenu()
            openAddSearchEngineMenu()
        }.clickCustomSearchSuggestionsLearnMoreLink {
            verifyUrl(
                "support.mozilla.org/en-US/kb/manage-my-default-search-engines-firefox-android?as=u&utm_source=inproduct",
            )
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2203310
    @Test
    fun editCustomSearchEngineTest() {
        val customSearchEngine = object {
            val title = "TestSearchEngine"
            val url = "http://localhost:${searchMockServer.port}/searchResults.html?search=%s"
            val newTitle = "NewEngineTitle"
        }

        addCustomSearchEngine(searchMockServer, customSearchEngine.title)
        restartApp(activityTestRule.activityRule)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            openDefaultSearchEngineMenu()
            verifyEngineListContains(customSearchEngine.title, shouldExist = true)
            openEngineOverflowMenu(customSearchEngine.title)
            clickEdit()
            typeCustomEngineDetails(customSearchEngine.newTitle, customSearchEngine.url)
            saveEditSearchEngine()
            verifySnackBarText("Saved ${customSearchEngine.newTitle}")
            verifyEngineListContains(customSearchEngine.newTitle, shouldExist = true)
            pressBack()
            openManageShortcutsMenu()
            verifyEngineListContains(customSearchEngine.newTitle, shouldExist = true)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2203312
    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1848623")
    @Test
    fun verifyErrorMessagesForInvalidSearchEngineUrlsTest() {
        val customSearchEngine = object {
            val title = "TestSearchEngine"
            val badTemplateUrl = "http://localhost:${searchMockServer.port}/searchResults.html?search="
            val typoUrl = "http://local:${searchMockServer.port}/searchResults.html?search=%s"
            val goodUrl = "http://localhost:${searchMockServer.port}/searchResults.html?search=%s"
        }

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            openDefaultSearchEngineMenu()
            openAddSearchEngineMenu()
            typeCustomEngineDetails(customSearchEngine.title, customSearchEngine.badTemplateUrl)
            saveNewSearchEngine()
            verifyInvalidTemplateSearchStringFormatError()
            typeCustomEngineDetails(customSearchEngine.title, customSearchEngine.typoUrl)
            saveNewSearchEngine()
            verifyErrorConnectingToSearchString(customSearchEngine.title)
            typeCustomEngineDetails(customSearchEngine.title, customSearchEngine.goodUrl)
            typeSearchEngineSuggestionString(customSearchEngine.badTemplateUrl)
            saveNewSearchEngine()
            verifyInvalidTemplateSearchStringFormatError()
            typeSearchEngineSuggestionString(customSearchEngine.typoUrl)
            saveNewSearchEngine()
            verifyErrorConnectingToSearchString(customSearchEngine.title)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2203313
    @Test
    fun deleteCustomSearchEngineTest() {
        val customSearchEngineTitle = "TestSearchEngine"

        addCustomSearchEngine(mockWebServer, searchEngineName = customSearchEngineTitle)
        restartApp(activityTestRule.activityRule)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            openDefaultSearchEngineMenu()
            verifyEngineListContains(customSearchEngineTitle, shouldExist = true)
            openEngineOverflowMenu(customSearchEngineTitle)
            clickDeleteSearchEngine()
            verifySnackBarText("Deleted $customSearchEngineTitle")
            clickUndoSnackBarButton()
            verifyEngineListContains(customSearchEngineTitle, shouldExist = true)
            changeDefaultSearchEngine(customSearchEngineTitle)
            openEngineOverflowMenu(customSearchEngineTitle)
            clickDeleteSearchEngine()
            verifyEngineListContains(customSearchEngineTitle, shouldExist = false)
            verifyDefaultSearchEngineSelected("Google")
            pressBack()
            openManageShortcutsMenu()
            verifyEngineListContains(customSearchEngineTitle, shouldExist = false)
            exitMenu()
        }
        searchScreen {
            clickSearchSelectorButton()
            verifySearchShortcutListContains(customSearchEngineTitle, shouldExist = false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2203339
    @Test
    fun deleteCustomSearchShortcutTest() {
        val customSearchEngineTitle = "TestSearchEngine"

        addCustomSearchEngine(mockWebServer, searchEngineName = customSearchEngineTitle)
        restartApp(activityTestRule.activityRule)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            openManageShortcutsMenu()
            verifyEngineListContains(customSearchEngineTitle, shouldExist = true)
            openCustomShortcutOverflowMenu(activityTestRule, customSearchEngineTitle)
            clickDeleteSearchEngine(activityTestRule)
            verifyEngineListContains(customSearchEngineTitle, shouldExist = false)
            pressBack()
            openDefaultSearchEngineMenu()
            verifyEngineListContains(customSearchEngineTitle, shouldExist = false)
            exitMenu()
        }
        searchScreen {
            clickSearchSelectorButton()
            verifySearchShortcutListContains(customSearchEngineTitle, shouldExist = false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/233588
    // Test running on beta/release builds in CI:
    // caution when making changes to it, so they don't block the builds
    // Goes through the settings and changes the search suggestion toggle, then verifies it changes.
    @Ignore("Failing, see: https://github.com/mozilla-mobile/fenix/issues/23817")
    @SmokeTest
    @Test
    fun verifyShowSearchSuggestionsToggleTest() {
        homeScreen {
        }.openSearch {
            typeSearch("mozilla ")
            verifySearchEngineSuggestionResults(
                activityTestRule,
                "mozilla firefox",
                searchTerm = "mozilla ",
            )
        }.dismissSearchBar {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            toggleShowSearchSuggestions()
        }.goBack {
        }.goBack {
        }.openSearch {
            typeSearch("mozilla")
            verifySuggestionsAreNotDisplayed(activityTestRule, "mozilla firefox")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/464420
    // Tests the "Don't allow" option from private mode search suggestions onboarding dialog
    @Test
    fun doNotAllowSearchSuggestionsInPrivateBrowsingTest() {
        homeScreen {
            togglePrivateBrowsingModeOnOff()
        }.openSearch {
            typeSearch("mozilla")
            verifyAllowSuggestionsInPrivateModeDialog()
            denySuggestionsInPrivateMode()
            verifySuggestionsAreNotDisplayed(activityTestRule, "mozilla firefox")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1957063
    // Tests the "Allow" option from private mode search suggestions onboarding dialog
    @Test
    fun allowSearchSuggestionsInPrivateBrowsingTest() {
        homeScreen {
            togglePrivateBrowsingModeOnOff()
        }.openSearch {
            typeSearch("mozilla")
            verifyAllowSuggestionsInPrivateModeDialog()
            allowSuggestionsInPrivateMode()
            verifySearchEngineSuggestionResults(
                activityTestRule,
                "mozilla firefox",
                searchTerm = "mozilla",
            )
        }.dismissSearchBar {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            switchShowSuggestionsInPrivateSessionsToggle()
        }.goBack {
        }.goBack {
        }.openSearch {
            typeSearch("mozilla")
            verifySuggestionsAreNotDisplayed(activityTestRule, "mozilla firefox")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/888673
    @Test
    fun verifyShowVoiceSearchToggleTest() {
        homeScreen {
        }.openSearch {
            verifyVoiceSearchButtonVisibility(true)
            startVoiceSearch()
        }.dismissSearchBar {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            toggleVoiceSearch()
            exitMenu()
        }
        homeScreen {
        }.openSearch {
            verifyVoiceSearchButtonVisibility(false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/412927
    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1807268")
    @Test
    fun verifyShowClipboardSuggestionsToggleTest() {
        val link = "https://www.mozilla.org/en-US/"
        setTextToClipBoard(appContext, link)

        homeScreen {
        }.openNavigationToolbar {
            verifyClipboardSuggestionsAreDisplayed(link, true)
        }.visitLinkFromClipboard {
            waitForPageToLoad()
        }.openTabDrawer {
        }.openNewTab {
        }
        navigationToolbar {
            // After visiting the link from clipboard it shouldn't be displayed again
            verifyClipboardSuggestionsAreDisplayed(shouldBeDisplayed = false)
        }.goBackToHomeScreen {
            setTextToClipBoard(appContext, link)
        }.openTabDrawer {
        }.openNewTab {
        }
        navigationToolbar {
            verifyClipboardSuggestionsAreDisplayed(link, true)
        }.goBackToHomeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            verifyShowClipboardSuggestionsEnabled(true)
            toggleClipboardSuggestion()
            verifyShowClipboardSuggestionsEnabled(false)
            exitMenu()
        }
        homeScreen {
        }.openTabDrawer {
        }.openNewTab {
        }
        navigationToolbar {
            verifyClipboardSuggestionsAreDisplayed(link, false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2233337
    @Test
    fun verifyTheSearchEnginesListsRespectTheLocaleTest() {
        runWithSystemLocaleChanged(Locale.CHINA, activityTestRule.activityRule) {
            // Checking search engines for CH locale
            homeScreen {
            }.openSearch {
                clickSearchSelectorButton()
                verifySearchShortcutListContains(
                    "Google",
                    "百度",
                    "Bing",
                    "DuckDuckGo",
                )
            }.dismissSearchBar {}

            // Checking search engines for FR locale
            setSystemLocale(Locale.FRENCH)
            homeScreen {
            }.openSearch {
                clickSearchSelectorButton()
                verifySearchShortcutListContains(
                    "Google",
                    "Bing",
                    "DuckDuckGo",
                    "Qwant",
                    "Wikipédia (fr)",
                )
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2203334
    @Test
    fun verifyManageSearchShortcutsSettingsItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            openManageShortcutsMenu()
            verifyToolbarText("Manage alternative search engines")
            verifyEnginesShortcutsListHeader()
            verifyManageShortcutsList(activityTestRule)
            verifySearchShortcutChecked(
                EngineShortcut(name = "Google", checkboxIndex = 1, isChecked = true),
                EngineShortcut(name = "Bing", checkboxIndex = 4, isChecked = true),
                EngineShortcut(name = "Amazon.com", checkboxIndex = 7, isChecked = true),
                EngineShortcut(name = "DuckDuckGo", checkboxIndex = 10, isChecked = true),
                EngineShortcut(name = "eBay", checkboxIndex = 13, isChecked = true),
                EngineShortcut(name = "Wikipedia", checkboxIndex = 16, isChecked = true),
                EngineShortcut(name = "Reddit", checkboxIndex = 19, isChecked = false),
                EngineShortcut(name = "YouTube", checkboxIndex = 22, isChecked = false),
            )
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2203340
    @SmokeTest
    @Test
    fun verifySearchShortcutChangesAreReflectedInSearchSelectorMenuTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            openManageShortcutsMenu()
            selectSearchShortcut(EngineShortcut(name = "Google", checkboxIndex = 1))
            selectSearchShortcut(EngineShortcut(name = "Amazon.com", checkboxIndex = 7))
            selectSearchShortcut(EngineShortcut(name = "Reddit", checkboxIndex = 19))
            selectSearchShortcut(EngineShortcut(name = "YouTube", checkboxIndex = 22))
            exitMenu()
        }
        searchScreen {
            clickSearchSelectorButton()
            verifySearchShortcutListContains("Google", "Amazon.com", shouldExist = false)
            verifySearchShortcutListContains("YouTube", shouldExist = true)
            verifySearchShortcutListContains("Reddit", shouldExist = true)
        }
    }
}
