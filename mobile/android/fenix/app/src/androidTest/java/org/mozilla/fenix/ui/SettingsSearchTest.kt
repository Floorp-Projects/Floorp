package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.FeatureSettingsHelper
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

class SettingsSearchTest {
    private lateinit var mockWebServer: MockWebServer
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get:Rule
    val activityTestRule = AndroidComposeTestRule(
        HomeActivityIntentTestRule()
    ) { it.activity }

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
        featureSettingsHelper.setJumpBackCFREnabled(false)
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()

        // resetting modified features enabled setting to default
        featureSettingsHelper.resetAllFeatureFlags()
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
            verifySearchEngineSuggestionResults(activityTestRule, "Test_Page_1")
            verifySearchEngineSuggestionResults(activityTestRule, "Test_Page_2")
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
            verifyNoSuggestionsAreDisplayed(activityTestRule, "Test_Page_1")
            verifyNoSuggestionsAreDisplayed(activityTestRule, "Test_Page_2")
        }
    }

    @SmokeTest
    @Test
    // Ads a new search engine from the list of custom engines
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

    @SmokeTest
    @Test
    // Verifies setting as default a customized search engine name and URL
    @Ignore("Failing intermittently https://github.com/mozilla-mobile/fenix/issues/22256")
    fun editCustomSearchEngineTest() {
        val searchEngine = object {
            var title = "Elefant"
            var url = "https://www.elefant.ro/search?SearchTerm=%s"
            var newTitle = "Test"
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

    @SmokeTest
    @Test
    // Test running on beta/release builds in CI:
    // caution when making changes to it, so they don't block the builds
    // Goes through the settings and changes the search suggestion toggle, then verifies it changes.
    fun toggleSearchSuggestionsTest() {

        homeScreen {
        }.openSearch {
            typeSearch("mozilla")
            verifySearchEngineSuggestionResults(activityTestRule, "mozilla firefox")
        }.dismissSearchBar {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            disableShowSearchSuggestions()
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
}
