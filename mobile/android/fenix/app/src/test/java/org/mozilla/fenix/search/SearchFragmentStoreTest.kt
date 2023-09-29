/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.verify
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.metrics.MetricsUtils
import org.mozilla.fenix.utils.Settings

class SearchFragmentStoreTest {

    @MockK private lateinit var searchEngine: SearchEngine

    @MockK private lateinit var activity: HomeActivity

    @MockK(relaxed = true)
    private lateinit var components: Components

    @MockK(relaxed = true)
    private lateinit var settings: Settings

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        every { activity.browsingModeManager } returns object : BrowsingModeManager {
            override var mode: BrowsingMode = BrowsingMode.Normal
        }
        every { components.settings } returns settings
    }

    @Test
    fun `createInitialSearchFragmentState with no tab in normal browsing mode`() {
        activity.browsingModeManager.mode = BrowsingMode.Normal
        every { components.core.store.state } returns BrowserState()
        every { settings.shouldShowSearchShortcuts } returns true
        every { settings.showUnifiedSearchFeature } returns true
        every { settings.shouldShowHistorySuggestions } returns true
        every { settings.shouldShowSearchSuggestions } returns true
        every { settings.shouldShowSearchSuggestionsInPrivate } returns false
        every { settings.showSponsoredSuggestions } returns true
        every { settings.showNonSponsoredSuggestions } returns true

        mockkStatic("org.mozilla.fenix.search.SearchFragmentStoreKt") {
            val expected = SearchFragmentState(
                query = "",
                url = "",
                searchTerms = "",
                searchEngineSource = SearchEngineSource.None,
                defaultEngine = null,
                showSearchShortcutsSetting = true,
                showSearchSuggestions = true,
                showSearchSuggestionsHint = false,
                showSearchShortcuts = false,
                areShortcutsAvailable = false,
                showClipboardSuggestions = false,
                showSearchTermHistory = true,
                showHistorySuggestionsForCurrentEngine = false,
                showAllHistorySuggestions = true,
                showBookmarksSuggestionsForCurrentEngine = false,
                showAllBookmarkSuggestions = false,
                showSyncedTabsSuggestionsForCurrentEngine = false,
                showAllSyncedTabsSuggestions = false,
                showSessionSuggestionsForCurrentEngine = false,
                showAllSessionSuggestions = true,
                showSponsoredSuggestions = true,
                showNonSponsoredSuggestions = true,
                tabId = null,
                pastedText = "pastedText",
                searchAccessPoint = MetricsUtils.Source.ACTION,
            )

            assertEquals(
                expected,
                createInitialSearchFragmentState(
                    activity,
                    components,
                    tabId = null,
                    pastedText = "pastedText",
                    searchAccessPoint = MetricsUtils.Source.ACTION,
                ),
            )
            assertEquals(
                expected.copy(tabId = "tabId"),
                createInitialSearchFragmentState(
                    activity,
                    components,
                    tabId = "tabId",
                    pastedText = "pastedText",
                    searchAccessPoint = MetricsUtils.Source.ACTION,
                ),
            )

            verify(exactly = 2) { shouldShowSearchSuggestions(BrowsingMode.Normal, settings) }
        }
    }

    @Test
    fun `createInitialSearchFragmentState with no tab in private browsing mode`() {
        activity.browsingModeManager.mode = BrowsingMode.Private
        every { components.core.store.state } returns BrowserState()
        every { settings.shouldShowSearchShortcuts } returns true
        every { settings.showUnifiedSearchFeature } returns true
        every { settings.shouldShowHistorySuggestions } returns true
        every { settings.shouldShowSearchSuggestions } returns true
        every { settings.shouldShowSearchSuggestionsInPrivate } returns false
        every { settings.showSponsoredSuggestions } returns true
        every { settings.showNonSponsoredSuggestions } returns true

        val expected = SearchFragmentState(
            query = "",
            url = "",
            searchTerms = "",
            searchEngineSource = SearchEngineSource.None,
            defaultEngine = null,
            showSearchShortcutsSetting = true,
            showSearchSuggestions = false,
            showSearchSuggestionsHint = false,
            showSearchShortcuts = false,
            areShortcutsAvailable = false,
            showClipboardSuggestions = false,
            showSearchTermHistory = true,
            showHistorySuggestionsForCurrentEngine = false,
            showAllHistorySuggestions = true,
            showBookmarksSuggestionsForCurrentEngine = false,
            showAllBookmarkSuggestions = false,
            showSyncedTabsSuggestionsForCurrentEngine = false,
            showAllSyncedTabsSuggestions = false,
            showSessionSuggestionsForCurrentEngine = false,
            showAllSessionSuggestions = true,
            showSponsoredSuggestions = false,
            showNonSponsoredSuggestions = false,
            tabId = null,
            pastedText = "pastedText",
            searchAccessPoint = MetricsUtils.Source.ACTION,
        )

        assertEquals(
            expected,
            createInitialSearchFragmentState(
                activity,
                components,
                tabId = null,
                pastedText = "pastedText",
                searchAccessPoint = MetricsUtils.Source.ACTION,
            ),
        )
    }

    @Test
    fun `createInitialSearchFragmentState with tab`() {
        activity.browsingModeManager.mode = BrowsingMode.Private
        every { components.core.store.state } returns BrowserState(
            tabs = listOf(
                TabSessionState(
                    id = "tabId",
                    content = ContentState(
                        url = "https://example.com",
                        searchTerms = "search terms",
                    ),
                ),
            ),
        )

        assertEquals(
            SearchFragmentState(
                query = "https://example.com",
                url = "https://example.com",
                searchTerms = "search terms",
                searchEngineSource = SearchEngineSource.None,
                defaultEngine = null,
                showSearchSuggestions = false,
                showSearchShortcutsSetting = false,
                showSearchSuggestionsHint = false,
                showSearchShortcuts = false,
                areShortcutsAvailable = false,
                showClipboardSuggestions = false,
                showSearchTermHistory = false,
                showHistorySuggestionsForCurrentEngine = false,
                showAllHistorySuggestions = false,
                showBookmarksSuggestionsForCurrentEngine = false,
                showAllBookmarkSuggestions = false,
                showSyncedTabsSuggestionsForCurrentEngine = false,
                showAllSyncedTabsSuggestions = false,
                showSessionSuggestionsForCurrentEngine = false,
                showAllSessionSuggestions = true,
                showSponsoredSuggestions = false,
                showNonSponsoredSuggestions = false,
                tabId = "tabId",
                pastedText = "",
                searchAccessPoint = MetricsUtils.Source.SHORTCUT,
            ),
            createInitialSearchFragmentState(
                activity,
                components,
                tabId = "tabId",
                pastedText = "",
                searchAccessPoint = MetricsUtils.Source.SHORTCUT,
            ),
        )
    }

    @Test
    fun updateQuery() = runTest {
        val initialState = emptyDefaultState()
        val store = SearchFragmentStore(initialState)
        val query = "test query"

        store.dispatch(SearchFragmentAction.UpdateQuery(query)).join()
        assertNotSame(initialState, store.state)
        assertEquals(query, store.state.query)
    }

    @Test
    fun `GIVEN search shortcuts are disabled and unified search is enabled in settings WHEN the default search engine is selected THEN search shortcuts are not displayed`() = runTest {
        val initialState = emptyDefaultState(showHistorySuggestionsForCurrentEngine = false)
        val store = SearchFragmentStore(initialState)
        every { settings.showUnifiedSearchFeature } returns true
        every { settings.shouldShowSearchShortcuts } returns false

        store.dispatch(
            SearchFragmentAction.SearchDefaultEngineSelected(
                engine = searchEngine,
                browsingMode = BrowsingMode.Normal,
                settings = settings,
            ),
        ).join()

        assertNotSame(initialState, store.state)
        assertFalse(store.state.showSearchShortcuts)
    }

    @Test
    fun `GIVEN search shortcuts are enabled and unified search is disabled in settings WHEN the default search engine is selected THEN search shortcuts are displayed`() = runTest {
        val initialState = emptyDefaultState(showHistorySuggestionsForCurrentEngine = false)
        val store = SearchFragmentStore(initialState)
        every { settings.showUnifiedSearchFeature } returns false
        every { settings.shouldShowSearchShortcuts } returns true

        store.dispatch(
            SearchFragmentAction.SearchDefaultEngineSelected(
                engine = searchEngine,
                browsingMode = BrowsingMode.Normal,
                settings = settings,
            ),
        ).join()

        assertNotSame(initialState, store.state)
        assertTrue(store.state.showSearchShortcuts)
    }

    @Test
    fun `GIVEN search shortcuts and unified search are both enabled in settings WHEN the default search engine is selected THEN search shortcuts are not displayed`() = runTest {
        val initialState = emptyDefaultState(showHistorySuggestionsForCurrentEngine = false)
        val store = SearchFragmentStore(initialState)
        every { settings.showUnifiedSearchFeature } returns true
        every { settings.shouldShowSearchShortcuts } returns true

        store.dispatch(
            SearchFragmentAction.SearchDefaultEngineSelected(
                engine = searchEngine,
                browsingMode = BrowsingMode.Normal,
                settings = settings,
            ),
        ).join()

        assertNotSame(initialState, store.state)
        assertFalse(store.state.showSearchShortcuts)
    }

    @Test
    fun `GIVEN search shortcuts and unified search are both disabled in settings WHEN the default search engine is selected THEN search shortcuts are not displayed`() = runTest {
        val initialState = emptyDefaultState(showHistorySuggestionsForCurrentEngine = false)
        val store = SearchFragmentStore(initialState)
        every { settings.showUnifiedSearchFeature } returns true
        every { settings.shouldShowSearchShortcuts } returns true

        store.dispatch(
            SearchFragmentAction.SearchDefaultEngineSelected(
                engine = searchEngine,
                browsingMode = BrowsingMode.Normal,
                settings = settings,
            ),
        ).join()

        assertNotSame(initialState, store.state)
        assertFalse(store.state.showSearchShortcuts)
    }

    // non default tests

    @Test
    fun `GIVEN search shortcuts are disabled and unified search is enabled in settings WHEN the search engine shortcut is selected THEN search shortcuts are not displayed`() = runTest {
        val initialState = emptyDefaultState(showHistorySuggestionsForCurrentEngine = false)
        val store = SearchFragmentStore(initialState)
        every { settings.shouldShowSearchShortcuts } returns false
        every { settings.showUnifiedSearchFeature } returns true

        val newEngine: SearchEngine = mockk {
            every { id } returns "DuckDuckGo"
            every { isGeneral } returns true
        }

        store.dispatch(
            SearchFragmentAction.SearchShortcutEngineSelected(
                engine = newEngine,
                browsingMode = BrowsingMode.Normal,
                settings = settings,
            ),
        ).join()

        assertNotSame(initialState, store.state)
        assertFalse(store.state.showSearchShortcuts)
    }

    @Test
    fun `GIVEN search shortcuts are enabled and unified search is disabled in settings WHEN the search engine shortcut is selected THEN search shortcuts are displayed`() = runTest {
        val initialState = emptyDefaultState(showHistorySuggestionsForCurrentEngine = false)
        val store = SearchFragmentStore(initialState)
        every { settings.shouldShowSearchShortcuts } returns true
        every { settings.showUnifiedSearchFeature } returns false

        val newEngine: SearchEngine = mockk {
            every { id } returns "DuckDuckGo"
            every { isGeneral } returns true
        }

        store.dispatch(
            SearchFragmentAction.SearchDefaultEngineSelected(
                engine = newEngine,
                browsingMode = BrowsingMode.Normal,
                settings = settings,
            ),
        ).join()

        assertNotSame(initialState, store.state)
        assertTrue(store.state.showSearchShortcuts)
    }

    @Test
    fun `GIVEN search shortcuts and unified search are both enabled in settings WHEN the search engine shortcut is selected THEN search shortcuts are not displayed`() = runTest {
        val initialState = emptyDefaultState(showHistorySuggestionsForCurrentEngine = false)
        val store = SearchFragmentStore(initialState)
        every { settings.shouldShowSearchShortcuts } returns true
        every { settings.showUnifiedSearchFeature } returns true

        val newEngine: SearchEngine = mockk {
            every { id } returns "DuckDuckGo"
            every { isGeneral } returns true
        }

        store.dispatch(
            SearchFragmentAction.SearchDefaultEngineSelected(
                engine = newEngine,
                browsingMode = BrowsingMode.Normal,
                settings = settings,
            ),
        ).join()

        assertNotSame(initialState, store.state)
        assertFalse(store.state.showSearchShortcuts)
    }

    @Test
    fun `GIVEN search shortcuts and unified search are both disabled in settings WHEN the search engine shortcut is selected THEN search shortcuts are not displayed`() = runTest {
        val initialState = emptyDefaultState(showHistorySuggestionsForCurrentEngine = false)
        val store = SearchFragmentStore(initialState)
        every { settings.shouldShowSearchShortcuts } returns true
        every { settings.showUnifiedSearchFeature } returns true

        val newEngine: SearchEngine = mockk {
            every { id } returns "DuckDuckGo"
            every { isGeneral } returns true
        }

        store.dispatch(
            SearchFragmentAction.SearchDefaultEngineSelected(
                engine = newEngine,
                browsingMode = BrowsingMode.Normal,
                settings = settings,
            ),
        ).join()

        assertNotSame(initialState, store.state)
        assertFalse(store.state.showSearchShortcuts)
    }

    @Test
    fun `GIVEN sponsored suggestions are enabled WHEN the default search engine is selected THEN sponsored suggestions are displayed`() = runTest {
        val initialState = emptyDefaultState(showSponsoredSuggestions = false, showNonSponsoredSuggestions = false)
        val store = SearchFragmentStore(initialState)
        every { settings.showSponsoredSuggestions } returns true
        every { settings.showNonSponsoredSuggestions } returns false

        store.dispatch(
            SearchFragmentAction.SearchDefaultEngineSelected(
                engine = searchEngine,
                browsingMode = BrowsingMode.Normal,
                settings = settings,
            ),
        ).join()

        assertNotSame(initialState, store.state)
        assertTrue(store.state.showSponsoredSuggestions)
        assertFalse(store.state.showNonSponsoredSuggestions)
    }

    @Test
    fun `GIVEN non-sponsored suggestions are enabled WHEN the default search engine is selected THEN non-sponsored suggestions are displayed`() = runTest {
        val initialState = emptyDefaultState(showSponsoredSuggestions = false, showNonSponsoredSuggestions = false)
        val store = SearchFragmentStore(initialState)
        every { settings.showSponsoredSuggestions } returns false
        every { settings.showNonSponsoredSuggestions } returns true

        store.dispatch(
            SearchFragmentAction.SearchDefaultEngineSelected(
                engine = searchEngine,
                browsingMode = BrowsingMode.Normal,
                settings = settings,
            ),
        ).join()

        assertNotSame(initialState, store.state)
        assertFalse(store.state.showSponsoredSuggestions)
        assertTrue(store.state.showNonSponsoredSuggestions)
    }

    @Test
    fun `GIVEN sponsored and non-sponsored suggestions are enabled WHEN the default search engine is selected THEN both are displayed`() = runTest {
        val initialState = emptyDefaultState(showSponsoredSuggestions = false, showNonSponsoredSuggestions = false)
        val store = SearchFragmentStore(initialState)
        every { settings.showSponsoredSuggestions } returns true
        every { settings.showNonSponsoredSuggestions } returns true

        store.dispatch(
            SearchFragmentAction.SearchDefaultEngineSelected(
                engine = searchEngine,
                browsingMode = BrowsingMode.Normal,
                settings = settings,
            ),
        ).join()

        assertNotSame(initialState, store.state)
        assertTrue(store.state.showSponsoredSuggestions)
        assertTrue(store.state.showNonSponsoredSuggestions)
    }

    @Test
    fun `GIVEN sponsored and non-sponsored suggestions are disabled WHEN the default search engine is selected THEN neither are displayed`() = runTest {
        val initialState = emptyDefaultState(showSponsoredSuggestions = true, showNonSponsoredSuggestions = true)
        val store = SearchFragmentStore(initialState)
        every { settings.showSponsoredSuggestions } returns false
        every { settings.showNonSponsoredSuggestions } returns false

        store.dispatch(
            SearchFragmentAction.SearchDefaultEngineSelected(
                engine = searchEngine,
                browsingMode = BrowsingMode.Normal,
                settings = settings,
            ),
        ).join()

        assertNotSame(initialState, store.state)
        assertFalse(store.state.showSponsoredSuggestions)
        assertFalse(store.state.showNonSponsoredSuggestions)
    }

    @Test
    fun `GIVEN sponsored and non-sponsored suggestions are enabled WHEN a shortcut is selected THEN neither are displayed`() = runTest {
        val initialState =
            emptyDefaultState(showSponsoredSuggestions = true, showNonSponsoredSuggestions = true)
        val store = SearchFragmentStore(initialState)
        every { settings.showSponsoredSuggestions } returns true
        every { settings.showNonSponsoredSuggestions } returns true

        store.dispatch(
            SearchFragmentAction.SearchShortcutEngineSelected(
                engine = searchEngine,
                browsingMode = BrowsingMode.Normal,
                settings = settings,
            ),
        ).join()

        assertNotSame(initialState, store.state)
        assertFalse(store.state.showSponsoredSuggestions)
        assertFalse(store.state.showNonSponsoredSuggestions)
    }

    @Test
    fun `GIVEN sponsored and non-sponsored suggestions are enabled WHEN the history engine is selected THEN neither are displayed`() = runTest {
        val initialState =
            emptyDefaultState(showSponsoredSuggestions = true, showNonSponsoredSuggestions = true)
        val store = SearchFragmentStore(initialState)
        every { settings.showSponsoredSuggestions } returns true
        every { settings.showNonSponsoredSuggestions } returns true

        store.dispatch(SearchFragmentAction.SearchHistoryEngineSelected(searchEngine)).join()

        assertNotSame(initialState, store.state)
        assertFalse(store.state.showSponsoredSuggestions)
        assertFalse(store.state.showNonSponsoredSuggestions)
    }

    @Test
    fun `GIVEN sponsored and non-sponsored suggestions are enabled WHEN the bookmarks engine is selected THEN neither are displayed`() = runTest {
        val initialState =
            emptyDefaultState(showSponsoredSuggestions = true, showNonSponsoredSuggestions = true)
        val store = SearchFragmentStore(initialState)
        every { settings.showSponsoredSuggestions } returns true
        every { settings.showNonSponsoredSuggestions } returns true

        store.dispatch(SearchFragmentAction.SearchBookmarksEngineSelected(searchEngine)).join()

        assertNotSame(initialState, store.state)
        assertFalse(store.state.showSponsoredSuggestions)
        assertFalse(store.state.showNonSponsoredSuggestions)
    }

    @Test
    fun `GIVEN sponsored and non-sponsored suggestions are enabled WHEN the tabs engine is selected THEN neither are displayed`() = runTest {
        val initialState =
            emptyDefaultState(showSponsoredSuggestions = true, showNonSponsoredSuggestions = true)
        val store = SearchFragmentStore(initialState)
        every { settings.showSponsoredSuggestions } returns true
        every { settings.showNonSponsoredSuggestions } returns true

        store.dispatch(SearchFragmentAction.SearchTabsEngineSelected(searchEngine)).join()

        assertNotSame(initialState, store.state)
        assertFalse(store.state.showSponsoredSuggestions)
        assertFalse(store.state.showNonSponsoredSuggestions)
    }

    @Test
    fun `GIVEN private browsing mode WHEN the search engine is the default one THEN search suggestions providers are updated`() = runTest {
        val initialState = emptyDefaultState(showHistorySuggestionsForCurrentEngine = false)
        val store = SearchFragmentStore(initialState)
        every { settings.shouldShowSearchShortcuts } returns false
        every { settings.shouldShowSearchSuggestions } returns true
        every { settings.shouldShowClipboardSuggestions } returns true
        every { settings.shouldShowHistorySuggestions } returns true
        every { settings.shouldShowBookmarkSuggestions } returns false
        every { settings.shouldShowSyncedTabsSuggestions } returns false
        every { settings.shouldShowSearchSuggestions } returns true
        every { settings.shouldShowSearchSuggestionsInPrivate } returns true
        every { settings.showSponsoredSuggestions } returns true
        every { settings.showNonSponsoredSuggestions } returns true

        mockkStatic("org.mozilla.fenix.search.SearchFragmentStoreKt") {
            store.dispatch(
                SearchFragmentAction.SearchDefaultEngineSelected(
                    engine = searchEngine,
                    browsingMode = BrowsingMode.Private,
                    settings = settings,
                ),
            ).join()

            assertNotSame(initialState, store.state)
            assertEquals(SearchEngineSource.Default(searchEngine), store.state.searchEngineSource)

            assertTrue(store.state.showSearchSuggestions)
            assertFalse(store.state.showSearchShortcuts)
            assertTrue(store.state.showClipboardSuggestions)
            assertFalse(store.state.showSearchTermHistory)
            assertFalse(store.state.showHistorySuggestionsForCurrentEngine)
            assertTrue(store.state.showAllHistorySuggestions)
            assertFalse(store.state.showAllBookmarkSuggestions)
            assertFalse(store.state.showAllSyncedTabsSuggestions)
            assertTrue(store.state.showAllSessionSuggestions)
            assertFalse(store.state.showSponsoredSuggestions)
            assertFalse(store.state.showNonSponsoredSuggestions)
            verify { shouldShowSearchSuggestions(BrowsingMode.Private, settings) }
        }
    }

    @Test
    fun `GIVEN normal browsing mode WHEN the search engine is the default one THEN search suggestions providers are updated`() = runTest {
        val initialState = emptyDefaultState(showHistorySuggestionsForCurrentEngine = false)
        val store = SearchFragmentStore(initialState)
        every { settings.shouldShowSearchShortcuts } returns false
        every { settings.shouldShowSearchSuggestions } returns true
        every { settings.shouldShowClipboardSuggestions } returns true
        every { settings.shouldShowHistorySuggestions } returns true
        every { settings.shouldShowBookmarkSuggestions } returns false
        every { settings.shouldShowSyncedTabsSuggestions } returns false
        every { settings.shouldShowSearchSuggestions } returns true
        every { settings.shouldShowSearchSuggestionsInPrivate } returns true
        every { settings.showSponsoredSuggestions } returns true
        every { settings.showNonSponsoredSuggestions } returns true

        store.dispatch(
            SearchFragmentAction.SearchDefaultEngineSelected(
                engine = searchEngine,
                browsingMode = BrowsingMode.Normal,
                settings = settings,
            ),
        ).join()

        assertNotSame(initialState, store.state)
        assertEquals(SearchEngineSource.Default(searchEngine), store.state.searchEngineSource)

        assertTrue(store.state.showSearchSuggestions)
        assertFalse(store.state.showSearchShortcuts)
        assertTrue(store.state.showClipboardSuggestions)
        assertFalse(store.state.showSearchTermHistory)
        assertFalse(store.state.showHistorySuggestionsForCurrentEngine)
        assertTrue(store.state.showAllHistorySuggestions)
        assertFalse(store.state.showAllBookmarkSuggestions)
        assertFalse(store.state.showAllSyncedTabsSuggestions)
        assertTrue(store.state.showAllSessionSuggestions)
        assertTrue(store.state.showSponsoredSuggestions)
        assertTrue(store.state.showNonSponsoredSuggestions)
    }

    @Test
    fun `GIVEN unified search is enabled WHEN the search engine is updated to a general engine shortcut THEN search suggestions providers are updated`() = runTest {
        val initialState = emptyDefaultState(showHistorySuggestionsForCurrentEngine = false)
        val store = SearchFragmentStore(initialState)
        val topicSpecificEngine: SearchEngine = mockk {
            every { isGeneral } returns false
        }
        every { settings.showUnifiedSearchFeature } returns true
        every { settings.shouldShowSearchShortcuts } returns true
        every { settings.shouldShowClipboardSuggestions } returns true
        every { settings.shouldShowHistorySuggestions } returns true
        every { settings.shouldShowBookmarkSuggestions } returns true
        every { settings.shouldShowSyncedTabsSuggestions } returns true
        every { settings.shouldShowSearchSuggestions } returns true
        every { settings.showSponsoredSuggestions } returns true
        every { settings.showNonSponsoredSuggestions } returns true

        store.dispatch(
            SearchFragmentAction.SearchShortcutEngineSelected(
                engine = topicSpecificEngine,
                browsingMode = BrowsingMode.Normal,
                settings = settings,
            ),
        ).join()

        assertNotSame(initialState, store.state)
        assertEquals(SearchEngineSource.Shortcut(topicSpecificEngine), store.state.searchEngineSource)
        assertTrue(store.state.showSearchSuggestions)
        assertFalse(store.state.showSearchShortcuts)
        assertTrue(store.state.showClipboardSuggestions)
        assertTrue(store.state.showSearchTermHistory)
        assertTrue(store.state.showHistorySuggestionsForCurrentEngine)
        assertFalse(store.state.showAllHistorySuggestions)
        assertTrue(store.state.showBookmarksSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllBookmarkSuggestions)
        assertTrue(store.state.showSyncedTabsSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllSyncedTabsSuggestions)
        assertTrue(store.state.showSessionSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllSessionSuggestions)
        assertFalse(store.state.showSponsoredSuggestions)
        assertFalse(store.state.showNonSponsoredSuggestions)

        every { settings.shouldShowSearchSuggestions } returns false
        val generalEngine: SearchEngine = mockk {
            every { isGeneral } returns true
        }
        store.dispatch(
            SearchFragmentAction.SearchShortcutEngineSelected(
                engine = generalEngine,
                browsingMode = BrowsingMode.Normal,
                settings = settings,
            ),
        ).join()
        assertNotSame(initialState, store.state)
        assertEquals(SearchEngineSource.Shortcut(generalEngine), store.state.searchEngineSource)
        assertFalse(store.state.showSearchSuggestions)
        assertFalse(store.state.showSearchShortcuts)
        assertTrue(store.state.showClipboardSuggestions)
        assertTrue(store.state.showSearchTermHistory)
        assertFalse(store.state.showHistorySuggestionsForCurrentEngine)
        assertFalse(store.state.showAllHistorySuggestions)
        assertFalse(store.state.showBookmarksSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllBookmarkSuggestions)
        assertFalse(store.state.showSyncedTabsSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllSyncedTabsSuggestions)
        assertFalse(store.state.showSessionSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllSessionSuggestions)
        assertFalse(store.state.showSponsoredSuggestions)
        assertFalse(store.state.showNonSponsoredSuggestions)
    }

    @Test
    fun `GIVEN unified search is enabled WHEN the search engine is updated to a topic specific engine shortcut THEN search suggestions providers are updated`() = runTest {
        val initialState = emptyDefaultState(showHistorySuggestionsForCurrentEngine = false)
        val store = SearchFragmentStore(initialState)
        every { searchEngine.isGeneral } returns false
        every { settings.showUnifiedSearchFeature } returns true
        every { settings.shouldShowSearchSuggestions } returns false
        every { settings.shouldShowSearchShortcuts } returns false
        every { settings.shouldShowClipboardSuggestions } returns false
        every { settings.shouldShowHistorySuggestions } returns true
        every { settings.shouldShowBookmarkSuggestions } returns false
        every { settings.shouldShowSyncedTabsSuggestions } returns false
        every { settings.showSponsoredSuggestions } returns true
        every { settings.showNonSponsoredSuggestions } returns true

        store.dispatch(
            SearchFragmentAction.SearchShortcutEngineSelected(
                engine = searchEngine,
                browsingMode = BrowsingMode.Normal,
                settings = settings,
            ),
        ).join()

        assertNotSame(initialState, store.state)
        assertEquals(SearchEngineSource.Shortcut(searchEngine), store.state.searchEngineSource)
        assertFalse(store.state.showSearchSuggestions)
        assertFalse(store.state.showSearchShortcuts)
        assertFalse(store.state.showClipboardSuggestions)
        assertTrue(store.state.showSearchTermHistory)
        assertTrue(store.state.showHistorySuggestionsForCurrentEngine)
        assertFalse(store.state.showAllHistorySuggestions)
        assertFalse(store.state.showBookmarksSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllBookmarkSuggestions)
        assertFalse(store.state.showSyncedTabsSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllSyncedTabsSuggestions)
        assertTrue(store.state.showSessionSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllSessionSuggestions)
        assertFalse(store.state.showSponsoredSuggestions)
        assertFalse(store.state.showNonSponsoredSuggestions)
    }

    @Test
    fun `GIVEN unified search is disabled WHEN the search engine is updated to a shortcut THEN search suggestions providers are updated`() = runTest {
        val initialState = emptyDefaultState(showHistorySuggestionsForCurrentEngine = false)
        val store = SearchFragmentStore(initialState)
        every { settings.showUnifiedSearchFeature } returns false
        every { settings.shouldShowSearchShortcuts } returns true
        every { settings.shouldShowClipboardSuggestions } returns false
        every { settings.shouldShowHistorySuggestions } returns true
        every { settings.shouldShowBookmarkSuggestions } returns false
        every { settings.shouldShowSyncedTabsSuggestions } returns true
        every { settings.shouldShowSearchSuggestions } returns true
        every { settings.shouldShowSearchSuggestionsInPrivate } returns true
        every { settings.showSponsoredSuggestions } returns true
        every { settings.showNonSponsoredSuggestions } returns true

        store.dispatch(
            SearchFragmentAction.SearchShortcutEngineSelected(
                engine = searchEngine,
                browsingMode = BrowsingMode.Private,
                settings = settings,
            ),
        ).join()

        assertNotSame(initialState, store.state)
        assertEquals(SearchEngineSource.Shortcut(searchEngine), store.state.searchEngineSource)
        assertTrue(store.state.showSearchSuggestions)
        assertTrue(store.state.showSearchShortcuts)
        assertFalse(store.state.showClipboardSuggestions)
        assertFalse(store.state.showSearchTermHistory)
        assertFalse(store.state.showHistorySuggestionsForCurrentEngine)
        assertTrue(store.state.showAllHistorySuggestions)
        assertFalse(store.state.showAllBookmarkSuggestions)
        assertTrue(store.state.showAllSyncedTabsSuggestions)
        assertTrue(store.state.showAllSessionSuggestions)
        assertFalse(store.state.showSponsoredSuggestions)
        assertFalse(store.state.showNonSponsoredSuggestions)
    }

    @Test
    fun `GIVEN unified search is enabled WHEN updating the search engine to a topic specific one THEN enable filtered bookmarks, history and tabs suggestions`() = runTest {
        val initialState = emptyDefaultState()
        val store = SearchFragmentStore(initialState)
        val topicSpecificEngine1: SearchEngine = mockk(relaxed = true) {
            every { name } returns "1"
            every { isGeneral } returns false
        }
        every { settings.showUnifiedSearchFeature } returns true

        every { settings.shouldShowBookmarkSuggestions } returns false
        every { settings.shouldShowSyncedTabsSuggestions } returns false
        store.dispatch(
            SearchFragmentAction.SearchShortcutEngineSelected(
                engine = topicSpecificEngine1,
                browsingMode = BrowsingMode.Private,
                settings = settings,
            ),
        ).join()
        assertNotSame(initialState, store.state)
        assertEquals(SearchEngineSource.Shortcut(topicSpecificEngine1), store.state.searchEngineSource)
        assertFalse(store.state.showBookmarksSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllBookmarkSuggestions)
        assertFalse(store.state.showSyncedTabsSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllSyncedTabsSuggestions)
        assertTrue(store.state.showSessionSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllSessionSuggestions)

        val topicSpecificEngine2 = topicSpecificEngine1.copy(
            name = "2",
        )
        every { settings.shouldShowBookmarkSuggestions } returns true
        every { settings.shouldShowSyncedTabsSuggestions } returns true
        store.dispatch(
            SearchFragmentAction.SearchShortcutEngineSelected(
                engine = topicSpecificEngine2,
                browsingMode = BrowsingMode.Private,
                settings = settings,
            ),
        ).join()
        assertNotSame(initialState, store.state)
        assertEquals(SearchEngineSource.Shortcut(topicSpecificEngine2), store.state.searchEngineSource)
        assertTrue(store.state.showBookmarksSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllBookmarkSuggestions)
        assertTrue(store.state.showSyncedTabsSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllSyncedTabsSuggestions)
        assertTrue(store.state.showSessionSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllSessionSuggestions)
    }

    @Test
    fun `GIVEN unified search is disabled WHEN updating the search engine to a topic specific one THEN enable bookmarks and tabs suggestions if user enabled`() = runTest {
        val initialState = emptyDefaultState()
        val store = SearchFragmentStore(initialState)
        val topicSpecificEngine1: SearchEngine = mockk(relaxed = true) {
            every { id } returns "1"
            every { isGeneral } returns false
        }
        every { settings.showUnifiedSearchFeature } returns true

        every { settings.shouldShowBookmarkSuggestions } returns false
        every { settings.shouldShowSyncedTabsSuggestions } returns true
        store.dispatch(
            SearchFragmentAction.SearchShortcutEngineSelected(
                engine = topicSpecificEngine1,
                browsingMode = BrowsingMode.Private,
                settings = settings,
            ),
        ).join()
        assertNotSame(initialState, store.state)
        assertEquals(SearchEngineSource.Shortcut(topicSpecificEngine1), store.state.searchEngineSource)
        assertFalse(store.state.showAllBookmarkSuggestions)
        assertTrue(store.state.showSyncedTabsSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllSyncedTabsSuggestions)
        assertTrue(store.state.showSessionSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllSessionSuggestions)

        val topicSpecificEngine2 = topicSpecificEngine1.copy(
            id = "2",
        )
        every { settings.shouldShowBookmarkSuggestions } returns true
        every { settings.shouldShowSyncedTabsSuggestions } returns false
        store.dispatch(
            SearchFragmentAction.SearchShortcutEngineSelected(
                engine = topicSpecificEngine2,
                browsingMode = BrowsingMode.Private,
                settings = settings,
            ),
        ).join()
        assertNotSame(initialState, store.state)
        assertEquals(SearchEngineSource.Shortcut(topicSpecificEngine2), store.state.searchEngineSource)
        assertTrue(store.state.showBookmarksSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllBookmarkSuggestions)
        assertFalse(store.state.showSyncedTabsSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllSyncedTabsSuggestions)
        assertTrue(store.state.showSessionSuggestionsForCurrentEngine)
        assertFalse(store.state.showAllSessionSuggestions)
    }

    @Test
    fun `WHEN doing a history search THEN search suggestions providers are updated`() = runTest {
        val initialState = emptyDefaultState(showHistorySuggestionsForCurrentEngine = true)
        val store = SearchFragmentStore(initialState)

        store.dispatch(SearchFragmentAction.SearchHistoryEngineSelected(searchEngine)).join()

        assertNotSame(initialState, store.state)
        assertEquals(SearchEngineSource.History(searchEngine), store.state.searchEngineSource)
        assertFalse(store.state.showSearchSuggestions)
        assertFalse(store.state.showSearchShortcuts)
        assertFalse(store.state.showClipboardSuggestions)
        assertFalse(store.state.showSearchTermHistory)
        assertFalse(store.state.showHistorySuggestionsForCurrentEngine)
        assertTrue(store.state.showAllHistorySuggestions)
        assertFalse(store.state.showAllBookmarkSuggestions)
        assertFalse(store.state.showAllSyncedTabsSuggestions)
        assertFalse(store.state.showAllSessionSuggestions)
        assertFalse(store.state.showSponsoredSuggestions)
        assertFalse(store.state.showNonSponsoredSuggestions)
    }

    @Test
    fun `WHEN doing a bookmarks search THEN search suggestions providers are updated`() = runTest {
        val initialState = emptyDefaultState(showHistorySuggestionsForCurrentEngine = true)
        val store = SearchFragmentStore(initialState)

        store.dispatch(SearchFragmentAction.SearchBookmarksEngineSelected(searchEngine)).join()

        assertNotSame(initialState, store.state)
        assertEquals(SearchEngineSource.Bookmarks(searchEngine), store.state.searchEngineSource)
        assertFalse(store.state.showSearchSuggestions)
        assertFalse(store.state.showSearchShortcuts)
        assertFalse(store.state.showClipboardSuggestions)
        assertFalse(store.state.showSearchTermHistory)
        assertFalse(store.state.showHistorySuggestionsForCurrentEngine)
        assertFalse(store.state.showAllHistorySuggestions)
        assertTrue(store.state.showAllBookmarkSuggestions)
        assertFalse(store.state.showAllSyncedTabsSuggestions)
        assertFalse(store.state.showAllSessionSuggestions)
        assertFalse(store.state.showSponsoredSuggestions)
        assertFalse(store.state.showNonSponsoredSuggestions)
    }

    @Test
    fun `WHEN doing a tabs search THEN search suggestions providers are updated`() = runTest {
        val initialState = emptyDefaultState(showHistorySuggestionsForCurrentEngine = true)
        val store = SearchFragmentStore(initialState)

        store.dispatch(SearchFragmentAction.SearchTabsEngineSelected(searchEngine)).join()

        assertNotSame(initialState, store.state)
        assertEquals(SearchEngineSource.Tabs(searchEngine), store.state.searchEngineSource)
        assertFalse(store.state.showSearchSuggestions)
        assertFalse(store.state.showSearchShortcuts)
        assertFalse(store.state.showClipboardSuggestions)
        assertFalse(store.state.showSearchTermHistory)
        assertFalse(store.state.showHistorySuggestionsForCurrentEngine)
        assertFalse(store.state.showAllHistorySuggestions)
        assertFalse(store.state.showAllBookmarkSuggestions)
        assertTrue(store.state.showAllSyncedTabsSuggestions)
        assertTrue(store.state.showAllSessionSuggestions)
        assertFalse(store.state.showSponsoredSuggestions)
        assertFalse(store.state.showNonSponsoredSuggestions)
    }

    @Test
    fun `WHEN tabs engine selected action dispatched THEN update search engine source`() = runTest {
        val initialState = emptyDefaultState()
        val store = SearchFragmentStore(initialState)

        store.dispatch(SearchFragmentAction.SearchTabsEngineSelected(searchEngine)).join()
        assertNotSame(initialState, store.state)
        assertEquals(SearchEngineSource.Tabs(searchEngine), store.state.searchEngineSource)
    }

    @Test
    fun showSearchSuggestions() = runTest {
        val initialState = emptyDefaultState()
        val store = SearchFragmentStore(initialState)

        store.dispatch(SearchFragmentAction.SetShowSearchSuggestions(true)).join()
        assertNotSame(initialState, store.state)
        assertTrue(store.state.showSearchSuggestions)

        store.dispatch(SearchFragmentAction.SetShowSearchSuggestions(false)).join()
        assertFalse(store.state.showSearchSuggestions)
    }

    @Test
    fun allowSearchInPrivateMode() = runTest {
        val initialState = emptyDefaultState()
        val store = SearchFragmentStore(initialState)

        store.dispatch(SearchFragmentAction.AllowSearchSuggestionsInPrivateModePrompt(true)).join()
        assertNotSame(initialState, store.state)
        assertTrue(store.state.showSearchSuggestionsHint)

        store.dispatch(SearchFragmentAction.AllowSearchSuggestionsInPrivateModePrompt(false)).join()
        assertFalse(store.state.showSearchSuggestionsHint)
    }

    @Test
    fun updatingClipboardUrl() {
        val initialState = emptyDefaultState()
        val store = SearchFragmentStore(initialState)

        assertFalse(store.state.clipboardHasUrl)

        store.dispatch(
            SearchFragmentAction.UpdateClipboardHasUrl(true),
        ).joinBlocking()

        assertTrue(store.state.clipboardHasUrl)
    }

    @Test
    fun `Updating SearchFragmentState from SearchState`() {
        val store = SearchFragmentStore(
            emptyDefaultState(
                searchEngineSource = SearchEngineSource.None,
                areShortcutsAvailable = false,
                defaultEngine = null,
                showSearchShortcutsSetting = true,
            ),
        )

        assertNull(store.state.defaultEngine)
        assertFalse(store.state.areShortcutsAvailable)
        assertFalse(store.state.showSearchShortcuts)
        assertEquals(SearchEngineSource.None, store.state.searchEngineSource)

        store.dispatch(
            SearchFragmentAction.UpdateSearchState(
                search = SearchState(
                    region = RegionState("US", "US"),
                    regionSearchEngines = listOf(
                        SearchEngine("engine-a", "Engine A", mockk(), type = SearchEngine.Type.BUNDLED),
                        SearchEngine("engine-b", "Engine B", mockk(), type = SearchEngine.Type.BUNDLED),
                        SearchEngine("engine-c", "Engine C", mockk(), type = SearchEngine.Type.BUNDLED),
                    ),
                    customSearchEngines = listOf(
                        SearchEngine("engine-d", "Engine D", mockk(), type = SearchEngine.Type.CUSTOM),
                        SearchEngine("engine-e", "Engine E", mockk(), type = SearchEngine.Type.CUSTOM),
                    ),
                    additionalSearchEngines = listOf(
                        SearchEngine("engine-f", "Engine F", mockk(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                    ),
                    additionalAvailableSearchEngines = listOf(
                        SearchEngine("engine-g", "Engine G", mockk(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                        SearchEngine("engine-h", "Engine H", mockk(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                    ),
                    hiddenSearchEngines = listOf(
                        SearchEngine("engine-i", "Engine I", mockk(), type = SearchEngine.Type.BUNDLED),
                    ),
                    regionDefaultSearchEngineId = "engine-b",
                    userSelectedSearchEngineId = null,
                    userSelectedSearchEngineName = null,
                ),
                isUnifiedSearchEnabled = false,
            ),
        )

        store.waitUntilIdle()

        assertNotNull(store.state.defaultEngine)
        assertEquals("Engine B", store.state.defaultEngine!!.name)

        assertTrue(store.state.areShortcutsAvailable)
        assertTrue(store.state.showSearchShortcuts)

        assertTrue(store.state.searchEngineSource is SearchEngineSource.Default)
        assertNotNull(store.state.searchEngineSource.searchEngine)
        assertEquals("Engine B", store.state.searchEngineSource.searchEngine!!.name)
    }

    @Test
    fun `Updating SearchFragmentState from SearchState - shortcuts disabled`() {
        val store = SearchFragmentStore(
            emptyDefaultState(
                searchEngineSource = SearchEngineSource.None,
                areShortcutsAvailable = false,
                defaultEngine = null,
                showSearchShortcutsSetting = false,
            ),
        )

        assertNull(store.state.defaultEngine)
        assertFalse(store.state.areShortcutsAvailable)
        assertFalse(store.state.showSearchShortcuts)
        assertEquals(SearchEngineSource.None, store.state.searchEngineSource)

        store.dispatch(
            SearchFragmentAction.UpdateSearchState(
                search = SearchState(
                    region = RegionState("US", "US"),
                    regionSearchEngines = listOf(
                        SearchEngine("engine-a", "Engine A", mockk(), type = SearchEngine.Type.BUNDLED),
                        SearchEngine("engine-b", "Engine B", mockk(), type = SearchEngine.Type.BUNDLED),
                        SearchEngine("engine-c", "Engine C", mockk(), type = SearchEngine.Type.BUNDLED),
                    ),
                    customSearchEngines = listOf(
                        SearchEngine("engine-d", "Engine D", mockk(), type = SearchEngine.Type.CUSTOM),
                        SearchEngine("engine-e", "Engine E", mockk(), type = SearchEngine.Type.CUSTOM),
                    ),
                    additionalSearchEngines = listOf(
                        SearchEngine("engine-f", "Engine F", mockk(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                    ),
                    additionalAvailableSearchEngines = listOf(
                        SearchEngine("engine-g", "Engine G", mockk(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                        SearchEngine("engine-h", "Engine H", mockk(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                    ),
                    hiddenSearchEngines = listOf(
                        SearchEngine("engine-i", "Engine I", mockk(), type = SearchEngine.Type.BUNDLED),
                    ),
                    regionDefaultSearchEngineId = "engine-b",
                    userSelectedSearchEngineId = null,
                    userSelectedSearchEngineName = null,
                ),
                isUnifiedSearchEnabled = false,
            ),
        )

        store.waitUntilIdle()

        assertNotNull(store.state.defaultEngine)
        assertEquals("Engine B", store.state.defaultEngine!!.name)

        assertTrue(store.state.areShortcutsAvailable)
        assertFalse(store.state.showSearchShortcuts)

        assertTrue(store.state.searchEngineSource is SearchEngineSource.Default)
        assertNotNull(store.state.searchEngineSource.searchEngine)
        assertEquals("Engine B", store.state.searchEngineSource.searchEngine!!.name)
    }

    @Test
    fun `GIVEN unified search is enabled WHEN updating the SearchFragmentState from SearchState THEN disable search shortcuts`() {
        val store = SearchFragmentStore(
            emptyDefaultState(
                searchEngineSource = SearchEngineSource.None,
                areShortcutsAvailable = false,
                defaultEngine = null,
                showSearchShortcutsSetting = false,
            ),
        )

        assertFalse(store.state.showSearchShortcuts)

        store.dispatch(
            SearchFragmentAction.UpdateSearchState(
                search = SearchState(
                    region = RegionState("US", "US"),
                    regionSearchEngines = listOf(
                        SearchEngine("engine-a", "Engine A", mockk(), type = SearchEngine.Type.BUNDLED),
                        SearchEngine("engine-b", "Engine B", mockk(), type = SearchEngine.Type.BUNDLED),
                    ),
                    customSearchEngines = listOf(),
                    additionalSearchEngines = listOf(),
                    additionalAvailableSearchEngines = listOf(),
                    hiddenSearchEngines = listOf(),
                    regionDefaultSearchEngineId = "engine-b",
                    userSelectedSearchEngineId = null,
                    userSelectedSearchEngineName = null,
                ),
                isUnifiedSearchEnabled = true,
            ),
        )
        store.waitUntilIdle()

        assertFalse(store.state.showSearchShortcuts)
    }

    @Test
    fun `GIVEN normal browsing mode and search suggestions enabled WHEN checking if search suggestions should be shown THEN return true`() {
        var settings: Settings = mockk {
            every { shouldShowSearchSuggestions } returns false
            every { shouldShowSearchSuggestionsInPrivate } returns false
        }
        assertFalse(shouldShowSearchSuggestions(BrowsingMode.Normal, settings))

        settings = mockk {
            every { shouldShowSearchSuggestions } returns true
            every { shouldShowSearchSuggestionsInPrivate } returns false
        }
        assertTrue(shouldShowSearchSuggestions(BrowsingMode.Normal, settings))
    }

    @Test
    fun `GIVEN private browsing mode and search suggestions enabled WHEN checking if search suggestions should be shown THEN return true`() {
        var settings: Settings = mockk {
            every { shouldShowSearchSuggestions } returns false
            every { shouldShowSearchSuggestionsInPrivate } returns false
        }
        assertFalse(shouldShowSearchSuggestions(BrowsingMode.Private, settings))

        settings = mockk {
            every { shouldShowSearchSuggestions } returns false
            every { shouldShowSearchSuggestionsInPrivate } returns true
        }
        assertFalse(shouldShowSearchSuggestions(BrowsingMode.Private, settings))

        settings = mockk {
            every { shouldShowSearchSuggestions } returns true
            every { shouldShowSearchSuggestionsInPrivate } returns true
        }
        assertTrue(shouldShowSearchSuggestions(BrowsingMode.Private, settings))
    }

    private fun emptyDefaultState(
        searchEngineSource: SearchEngineSource = mockk(),
        defaultEngine: SearchEngine? = mockk(),
        areShortcutsAvailable: Boolean = true,
        showSearchShortcutsSetting: Boolean = false,
        showHistorySuggestionsForCurrentEngine: Boolean = true,
        showSponsoredSuggestions: Boolean = true,
        showNonSponsoredSuggestions: Boolean = true,
    ): SearchFragmentState = SearchFragmentState(
        tabId = null,
        url = "",
        searchTerms = "",
        query = "",
        searchEngineSource = searchEngineSource,
        defaultEngine = defaultEngine,
        showSearchSuggestionsHint = false,
        showSearchShortcutsSetting = showSearchShortcutsSetting,
        showSearchSuggestions = false,
        showSearchShortcuts = false,
        areShortcutsAvailable = areShortcutsAvailable,
        showClipboardSuggestions = false,
        showSearchTermHistory = true,
        showHistorySuggestionsForCurrentEngine = showHistorySuggestionsForCurrentEngine,
        showAllHistorySuggestions = false,
        showBookmarksSuggestionsForCurrentEngine = false,
        showAllBookmarkSuggestions = false,
        showSyncedTabsSuggestionsForCurrentEngine = false,
        showAllSyncedTabsSuggestions = false,
        showSessionSuggestionsForCurrentEngine = false,
        showAllSessionSuggestions = false,
        showSponsoredSuggestions = showSponsoredSuggestions,
        showNonSponsoredSuggestions = showNonSponsoredSuggestions,
        searchAccessPoint = MetricsUtils.Source.NONE,
    )
}
