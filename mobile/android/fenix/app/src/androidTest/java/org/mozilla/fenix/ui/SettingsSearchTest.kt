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
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MockBrowserDataHelper.addCustomSearchEngine
import org.mozilla.fenix.helpers.SearchDispatcher
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestHelper.appContext
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.helpers.TestHelper.restartApp
import org.mozilla.fenix.helpers.TestHelper.runWithSystemLocaleChanged
import org.mozilla.fenix.helpers.TestHelper.setSystemLocale
import org.mozilla.fenix.helpers.TestHelper.setTextToClipBoard
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

    @Test
    fun searchSettingsItemsTest() {
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

    @Test
    fun defaultSearchEnginesSettingsItemsTest() {
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

    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1807268")
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
            verifySearchEngineSuggestionResults(
                activityTestRule,
                "Firefox Suggest",
                page1.title,
                searchTerm = "test",
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
            verifySearchEngineSuggestionResults(
                activityTestRule,
                "Firefox Suggest",
                website.title,
                searchTerm = "test",
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
                website.title,
            )
        }
    }

    // Verifies setting as default a customized search engine name and URL
    @SmokeTest
    @Test
    fun addCustomDefaultSearchEngineTest() {
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

    @Test
    fun addSearchEngineToManageShortcutsListTest() {
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

    @Test
    fun addSearchEngineLearnMoreLinksTest() {
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

    @Test
    fun errorForInvalidSearchEngineStringsTest() {
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

    // Test running on beta/release builds in CI:
    // caution when making changes to it, so they don't block the builds
    // Goes through the settings and changes the search suggestion toggle, then verifies it changes.
    @Ignore("Failing, see: https://github.com/mozilla-mobile/fenix/issues/23817")
    @SmokeTest
    @Test
    fun toggleSearchSuggestionsTest() {
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

    // Expected for app language set to Arabic
    @Test
    fun verifySearchEnginesWithRTLLocale() {
        val arabicLocale = Locale("ar", "AR")

        runWithSystemLocaleChanged(arabicLocale, activityTestRule.activityRule) {
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

    @Test
    fun searchEnginesListRespectLocaleTest() {
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

    @Test
    fun manageSearchShortcutsSettingsItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            openManageShortcutsMenu()
            verifyToolbarText("Manage search shortcuts")
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

    @SmokeTest
    @Test
    fun changeSearchShortcutsListTest() {
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
