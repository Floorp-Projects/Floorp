package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
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
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

class SettingsSearchTest {
    private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
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
    fun toggleShowVisitedSitesAndBookmarks() {
        // Bookmarks a few websites, toggles the history and bookmarks setting to off, then verifies if the visited and bookmarked websites do not show in the suggestions.
        val page1 = getGenericAsset(mockWebServer, 1)
        val page2 = getGenericAsset(mockWebServer, 2)
        val page3 = getGenericAsset(mockWebServer, 3)

        homeScreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(page1.url) {
        }.openThreeDotMenu {
        }.bookmarkPage { }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(page2.url) {
            verifyUrl(page2.url.toString())
        }.openThreeDotMenu {
        }.bookmarkPage { }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(page3.url) {
            mDevice.waitForIdle()
        }

        navigationToolbar {
            verifyNoHistoryBookmarks()
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
}
