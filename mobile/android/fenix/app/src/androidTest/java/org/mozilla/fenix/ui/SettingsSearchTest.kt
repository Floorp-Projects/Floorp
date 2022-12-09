package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.RecyclerViewIdlingResource
import org.mozilla.fenix.helpers.SearchDispatcher
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.appContext
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.helpers.TestHelper.setTextToClipBoard
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.util.ARABIC_LANGUAGE_HEADER

class SettingsSearchTest {
    private lateinit var mockWebServer: MockWebServer
    private lateinit var searchMockServer: MockWebServer

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
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    @Test
    fun searchSettingsItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            verifySearchToolbar()
            verifyDefaultSearchEngineHeader()
            verifySearchEngineList()
            verifyShowSearchSuggestions()
            verifyShowSearchShortcuts()
            verifySearchBrowsingHistory()
            verifySearchBookmarks()
            verifyShowClipboardSuggestionsDefault()
        }
    }

    @Test
    fun selectNewDefaultSearchEngine() {
        // Goes through the settings and changes the default search engine, then verifies it has changed.
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            changeDefaultSearchEngine("DuckDuckGo")
        }.goBack {
        }.goBack {
            verifyDefaultSearchEngine("DuckDuckGo")
        }
    }

    @Test
    fun toggleSearchAutocomplete() {
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

    @Test
    fun toggleSearchBookmarksAndHistoryTest() {
        // Bookmarks 2 websites, toggles the bookmarks and history search settings off,
        // then verifies if the websites do not show in the suggestions.
        val page1 = getGenericAsset(mockWebServer, 1)
        val page2 = getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(page1.url) {
            verifyUrl(page1.url.toString())
        }.openThreeDotMenu {
        }.bookmarkPage {
        }.openTabDrawer {
            closeTab()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(page2.url) {
            verifyUrl(page2.url.toString())
        }.openThreeDotMenu {
        }.bookmarkPage {
        }.openTabDrawer {
            closeTab()
        }
        // Verifies that bookmarks & history suggestions are shown
        homeScreen {
        }.openSearch {
            typeSearch("test")
            expandSearchSuggestionsList()
            verifyFirefoxSuggestResults(activityTestRule, "Firefox Suggest")
            verifyFirefoxSuggestResults(activityTestRule, "Test_Page_1")
            verifyFirefoxSuggestResults(activityTestRule, "Test_Page_2")
        }.dismissSearchBar {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            // Disables the search bookmarks & history settings
            verifySearchBookmarks()
            switchSearchBookmarksToggle()
            switchSearchHistoryToggle()
            exitMenu()
        }
        // Verifies that bookmarks and history suggestions are not shown
        homeScreen {
        }.openSearch {
            typeSearch("test")
            expandSearchSuggestionsList()
            verifyNoSuggestionsAreDisplayed(activityTestRule, "Firefox Suggest")
            verifyNoSuggestionsAreDisplayed(activityTestRule, "Test_Page_1")
            verifyNoSuggestionsAreDisplayed(activityTestRule, "Test_Page_2")
        }
    }

    // Ads a new search engine from the list of custom engines
    @SmokeTest
    @Test
    fun addPredefinedSearchEngineTest() {
        val searchEngine = "Reddit"

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            openAddSearchEngineMenu()
            verifyAddSearchEngineList()
            addNewSearchEngine(searchEngine)
            verifyEngineListContains(searchEngine)
        }.goBack {
        }.goBack {
        }.openSearch {
            verifyKeyboardVisibility()
            clickSearchEngineShortcutButton()
            verifyEnginesListShortcutContains(activityTestRule, searchEngine)
            changeDefaultSearchEngine(activityTestRule, searchEngine)
        }.submitQuery("mozilla ") {
            verifyUrl(searchEngine)
        }
    }

    // Verifies setting as default a customized search engine name and URL
    @SmokeTest
    @Test
    fun editCustomSearchEngineTest() {
        searchMockServer = MockWebServer().apply {
            dispatcher = SearchDispatcher()
            start()
        }
        val searchEngine = object {
            val title = "TestSearchEngine"
            val url = "http://localhost:${searchMockServer.port}/searchResults.html?search=%s"
            val newTitle = "Test"
        }

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            openAddSearchEngineMenu()
            selectAddCustomSearchEngine()
            typeCustomEngineDetails(searchEngine.title, searchEngine.url)
            saveNewSearchEngine()
            openEngineOverflowMenu(searchEngine.title)
            clickEdit()
            typeCustomEngineDetails(searchEngine.newTitle, searchEngine.url)
            saveEditSearchEngine()
            changeDefaultSearchEngine(searchEngine.newTitle)
        }.goBack {
        }.goBack {
        }.openSearch {
            verifyDefaultSearchEngine(searchEngine.newTitle)
            clickSearchEngineShortcutButton()
            verifyEnginesListShortcutContains(activityTestRule, searchEngine.newTitle)
        }
    }

    // Test running on beta/release builds in CI:
    // caution when making changes to it, so they don't block the builds
    // Goes through the settings and changes the search suggestion toggle, then verifies it changes.
    @Ignore("Failing, see: https://github.com/mozilla-mobile/fenix/issues/23817")
    @SmokeTest
    @Test
    fun toggleSearchSuggestionsTest() {
        homeScreen {
        }.openSearch {
            typeSearch("mozilla")
            verifySearchEngineSuggestionResults(activityTestRule, "mozilla firefox")
        }.dismissSearchBar {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            toggleShowSearchSuggestions()
        }.goBack {
        }.goBack {
        }.openSearch {
            typeSearch("mozilla")
            verifyNoSuggestionsAreDisplayed(activityTestRule, "mozilla firefox")
        }
    }

    // Tests the "Don't allow" option from private mode search suggestions onboarding dialog
    @Test
    fun blockSearchSuggestionsInPrivateModeOnboardingTest() {
        homeScreen {
            togglePrivateBrowsingModeOnOff()
        }.openSearch {
            typeSearch("mozilla")
            verifyAllowSuggestionsInPrivateModeDialog()
            denySuggestionsInPrivateMode()
            verifyNoSuggestionsAreDisplayed(activityTestRule, "mozilla firefox")
        }.dismissSearchBar {
            togglePrivateBrowsingModeOnOff()
        }.openSearch {
            typeSearch("mozilla")
            verifySearchEngineSuggestionResults(activityTestRule, "mozilla firefox")
        }
    }

    // Tests the "Allow" option from private mode search suggestions onboarding dialog
    @Test
    fun allowSearchSuggestionsInPrivateModeOnboardingTest() {
        homeScreen {
            togglePrivateBrowsingModeOnOff()
        }.openSearch {
            typeSearch("mozilla")
            verifyAllowSuggestionsInPrivateModeDialog()
            allowSuggestionsInPrivateMode()
            verifySearchEngineSuggestionResults(activityTestRule, "mozilla firefox")
        }.dismissSearchBar {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            toggleShowSuggestionsInPrivateSessions()
        }.goBack {
        }.goBack {
        }.openSearch {
            typeSearch("mozilla")
            verifyNoSuggestionsAreDisplayed(activityTestRule, "mozilla firefox")
        }
    }

    @SmokeTest
    @Test
    fun toggleVoiceSearchTest() {
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

    @Test
    fun toggleShowClipboardSuggestionsTest() {
        val link = "https://www.mozilla.org/en-US/"
        setTextToClipBoard(appContext, link)

        homeScreen {
        }.openNavigationToolbar {
            verifyClipboardSuggestionsAreDisplayed(link, true)
        }.visitLinkFromClipboard {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            verifyShowClipboardSuggestionsDefault()
            toggleClipboardSuggestion()
            exitMenu()
        }
        homeScreen {
        }.openNavigationToolbar {
            verifyClipboardSuggestionsAreDisplayed(link, false)
        }
    }

    // Expected for en-us defaults
    @Test
    fun undoDeleteSearchEngineTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            verifyEngineListContains("Bing")
            openEngineOverflowMenu("Bing")
            clickDeleteSearchEngine()
            clickUndoSnackBarButton()
            verifyEngineListContains("Bing")
        }
    }

    // Expected for en-us defaults
    @Test
    fun deleteDefaultSearchEngineTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            verifyEngineListContains("Google")
            verifyDefaultSearchEngine("Google")
            openEngineOverflowMenu("Google")
            clickDeleteSearchEngine()
            verifyEngineListDoesNotContain("Google")
            verifyDefaultSearchEngine("Bing")
        }
    }

    // Expected for en-us defaults
    @Test
    fun deleteAllSearchEnginesTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            deleteMultipleSearchEngines(
                "Google",
                "Bing",
                "Amazon.com",
                "DuckDuckGo",
                "eBay",
            )
            verifyDefaultSearchEngine("Wikipedia")
            verifyThreeDotButtonIsNotDisplayed("Wikipedia")
            openAddSearchEngineMenu()
            verifyAddSearchEngineListContains(
                "Google",
                "Bing",
                "Amazon.com",
                "DuckDuckGo",
                "eBay",
            )
        }
    }

    // Expected for en-us defaults
    @Test
    fun changeSearchEnginesBasedOnTextTest() {
        homeScreen {
        }.openSearch {
            typeSearch("D")
            verifySearchEnginePrompt(activityTestRule, "DuckDuckGo")
            clickSearchEnginePrompt(activityTestRule, "DuckDuckGo")
        }.submitQuery("firefox") {
            verifyUrl("duckduckgo.com/?q=firefox")
        }
    }

    // Expected for app language set to Arabic
    @Test
    fun verifySearchEnginesWithRTLLocale() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            toggleShowSearchShortcuts()
        }.goBack {
        }.openLanguageSubMenu {
            TestHelper.registerAndCleanupIdlingResources(
                RecyclerViewIdlingResource(
                    activityTestRule.activity.findViewById(R.id.locale_list),
                    2,
                ),
            ) {
                selectLanguage("Arabic")
                verifyLanguageHeaderIsTranslated(ARABIC_LANGUAGE_HEADER)
            }
        }

        exitMenu()

        homeScreen {
        }.openSearch {
            verifyTranslatedFocusedNavigationToolbar("ابحث أو أدخِل عنوانا")
            verifySearchEngineShortcuts(
                activityTestRule,
                "Google",
                "Bing",
                "Amazon.com",
                "DuckDuckGo",
                "ويكيبيديا (ar)",
            )
            changeDefaultSearchEngine(activityTestRule, "ويكيبيديا (ar)")
        }.submitQuery("firefox") {
            verifyUrl("ar.m.wikipedia.org")
        }
    }

    // Expected for en-us defaults
    @Test
    fun toggleSearchEnginesShortcutListTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            verifyShowSearchEnginesToggleState(false)
            toggleShowSearchShortcuts()
            verifyShowSearchEnginesToggleState(true)
        }

        exitMenu()

        homeScreen {
        }.openSearch {
            verifySearchEngineShortcuts(
                activityTestRule,
                "Google",
                "Bing",
                "Amazon.com",
                "DuckDuckGo",
                "eBay",
                "Wikipedia",
            )
        }.clickSearchEngineSettings(activityTestRule) {
            toggleShowSearchShortcuts()
            verifyShowSearchEnginesToggleState(false)
        }

        exitMenu()

        homeScreen {
        }.openSearch {
            verifySearchEngineShortcutsAreNotDisplayed(
                activityTestRule,
                "Google",
                "Bing",
                "Amazon.com",
                "DuckDuckGo",
                "eBay",
                "Wikipedia",
            )
            clickSearchEngineShortcutButton()
            verifySearchEngineShortcuts(
                activityTestRule,
                "Google",
                "Bing",
                "Amazon.com",
                "DuckDuckGo",
                "eBay",
                "Wikipedia",
            )
        }
    }
}
