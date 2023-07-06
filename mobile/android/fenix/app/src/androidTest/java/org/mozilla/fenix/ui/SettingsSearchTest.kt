package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.test.espresso.Espresso.pressBack
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
import org.mozilla.fenix.helpers.TestHelper.verifySnackBarText
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.searchScreen
import org.mozilla.fenix.ui.util.ARABIC_LANGUAGE_HEADER

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
        HomeActivityIntentTestRule.withDefaultSettingsOverrides(isUnifiedSearchEnabled = true, newSearchSettingsEnabled = true),
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
            verifySearchSettingsToolbar()
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

    @SmokeTest
    @Test
    fun selectNewDefaultSearchEngine() {
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
    fun toggleSearchHistoryTest() {
        val page1 = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(page1.url) {
        }.openTabDrawer {
            closeTab()
        }

        homeScreen {
        }.openSearch {
            typeSearch("test")
            verifyFirefoxSuggestResults(
                activityTestRule,
                "test",
                "Firefox Suggest",
                "Test_Page_1",
            )
        }.clickSearchSuggestion("Test_Page_1") {
            verifyUrl(page1.url.toString())
        }.openTabDrawer {
            closeTab()
        }

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
            verifyNoSuggestionsAreDisplayed(
                activityTestRule,
                "Firefox Suggest",
                "Test_Page_1",
            )
        }
    }

    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1807268")
    @Test
    fun toggleSearchBookmarksTest() {
        val website = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(website.url) {
        }.openThreeDotMenu {
        }.bookmarkPage {
        }.openTabDrawer {
            closeTab()
        }

        homeScreen {
        }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryListExists()
            clickDeleteHistoryButton("Test_Page_1")
            exitMenu()
        }

        homeScreen {
        }.openSearch {
            typeSearch("test")
            verifyFirefoxSuggestResults(
                activityTestRule,
                "test",
                "Firefox Suggest",
                "Test_Page_1",
            )
        }.clickSearchSuggestion("Test_Page_1") {
            verifyUrl(website.url.toString())
        }.openTabDrawer {
            closeTab()
        }

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
            verifyNoSuggestionsAreDisplayed(
                activityTestRule,
                "Firefox Suggest",
                "Test_Page_1",
            )
        }
    }

    // Verifies setting as default a customized search engine name and URL
    @SmokeTest
    @Test
    fun addCustomDefaultSearchEngineTest() {
        searchMockServer = MockWebServer().apply {
            dispatcher = SearchDispatcher()
            start()
        }
        val searchEngine = object {
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
            typeCustomEngineDetails(searchEngine.title, searchEngine.url)
            verifySaveSearchEngineButtonEnabled(true)
            saveNewSearchEngine()
            verifyEngineListContains(searchEngine.title)
            openEngineOverflowMenu(searchEngine.title)
            pressBack()
            changeDefaultSearchEngine(searchEngine.title)
            pressBack()
            openManageShortcutsMenu()
            verifyEngineListContains(searchEngine.title)
            pressBack()
        }.goBack {
            verifySettingsOptionSummary("Search", searchEngine.title)
        }.goBack {
        }.openSearch {
            verifySearchEngineIcon(searchEngine.title)
            clickSearchSelectorButton()
            verifySearchShortcutListContains(searchEngine.title)
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
            switchShowSuggestionsInPrivateSessionsToggle()
        }.goBack {
        }.goBack {
        }.openSearch {
            typeSearch("mozilla")
            verifyNoSuggestionsAreDisplayed(activityTestRule, "mozilla firefox")
        }
    }

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
            verifyShowClipboardSuggestionsEnabled(true)
            toggleClipboardSuggestion()
            verifyShowClipboardSuggestionsEnabled(false)
            exitMenu()
        }
        homeScreen {
        }.openNavigationToolbar {
            verifyClipboardSuggestionsAreDisplayed(link, false)
        }
    }

    @Test
    fun deleteCustomSearchEngineTest() {
        searchMockServer = MockWebServer().apply {
            dispatcher = SearchDispatcher()
            start()
        }
        val searchEngine = object {
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
            typeCustomEngineDetails(searchEngine.title, searchEngine.url)
            verifySaveSearchEngineButtonEnabled(true)
            saveNewSearchEngine()
            verifyEngineListContains(searchEngine.title)
            openEngineOverflowMenu(searchEngine.title)
            clickDeleteSearchEngine()
            clickUndoSnackBarButton()
            verifyEngineListContains(searchEngine.title)
            changeDefaultSearchEngine(searchEngine.title)
            openEngineOverflowMenu(searchEngine.title)
            clickDeleteSearchEngine()
            verifySnackBarText("Deleted ${searchEngine.title}")
            verifyEngineListDoesNotContain(searchEngine.title)
        }
    }

    // Expected for app language set to Arabic
    @Test
    fun verifySearchEnginesWithRTLLocale() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
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
            clickSearchSelectorButton()
            verifySearchShortcutListContains(
                "Google",
                "Bing",
                "Amazon.com",
                "DuckDuckGo",
                "ويكيبيديا (ar)",
            )
            selectTemporarySearchMethod("ويكيبيديا (ar)")
        }.submitQuery("firefox") {
            verifyUrl("firefox")
        }
    }
}
