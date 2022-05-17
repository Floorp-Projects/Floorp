/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Ignore
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
    @MockK(relaxed = true) private lateinit var components: Components
    @MockK(relaxed = true) private lateinit var settings: Settings

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        every { activity.browsingModeManager } returns object : BrowsingModeManager {
            override var mode: BrowsingMode = BrowsingMode.Normal
        }
        every { components.settings } returns settings
    }

    @Test
    fun `createInitialSearchFragmentState with no tab`() {
        activity.browsingModeManager.mode = BrowsingMode.Normal
        every { components.core.store.state } returns BrowserState()
        every { settings.shouldShowSearchShortcuts } returns true

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
            showHistorySuggestions = false,
            showBookmarkSuggestions = false,
            showSyncedTabsSuggestions = false,
            showSessionSuggestions = true,
            tabId = null,
            pastedText = "pastedText",
            searchAccessPoint = MetricsUtils.Source.ACTION
        )

        assertEquals(
            expected,
            createInitialSearchFragmentState(
                activity,
                components,
                tabId = null,
                pastedText = "pastedText",
                searchAccessPoint = MetricsUtils.Source.ACTION
            )
        )
        assertEquals(
            expected.copy(tabId = "tabId"),
            createInitialSearchFragmentState(
                activity,
                components,
                tabId = "tabId",
                pastedText = "pastedText",
                searchAccessPoint = MetricsUtils.Source.ACTION
            )
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
                        searchTerms = "search terms"
                    )
                )
            )
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
                showHistorySuggestions = false,
                showBookmarkSuggestions = false,
                showSyncedTabsSuggestions = false,
                showSessionSuggestions = true,
                tabId = "tabId",
                pastedText = "",
                searchAccessPoint = MetricsUtils.Source.SHORTCUT
            ),
            createInitialSearchFragmentState(
                activity,
                components,
                tabId = "tabId",
                pastedText = "",
                searchAccessPoint = MetricsUtils.Source.SHORTCUT
            )
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
    fun selectSearchShortcutEngine() = runTest {
        val initialState = emptyDefaultState()
        val store = SearchFragmentStore(initialState)

        store.dispatch(SearchFragmentAction.SearchShortcutEngineSelected(searchEngine, settings)).join()
        assertNotSame(initialState, store.state)
        assertEquals(SearchEngineSource.Shortcut(searchEngine), store.state.searchEngineSource)
        assertEquals(false, store.state.showSearchShortcuts)
    }

    @Test
    fun `WHEN history engine selected action dispatched THEN update search engine source`() = runTest {
        val initialState = emptyDefaultState()
        val store = SearchFragmentStore(initialState)

        store.dispatch(SearchFragmentAction.SearchHistoryEngineSelected(searchEngine)).join()
        assertNotSame(initialState, store.state)
        assertEquals(SearchEngineSource.History(searchEngine), store.state.searchEngineSource)
    }

    @Test
    fun showSearchShortcutEnginePicker() = runTest {
        val initialState = emptyDefaultState()
        val store = SearchFragmentStore(initialState)

        store.dispatch(SearchFragmentAction.ShowSearchShortcutEnginePicker(true)).join()
        assertNotSame(initialState, store.state)
        assertEquals(true, store.state.showSearchShortcuts)
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
            SearchFragmentAction.UpdateClipboardHasUrl(true)
        ).joinBlocking()

        assertTrue(store.state.clipboardHasUrl)
    }

    @Test
    @Ignore("Flaky, needs investigation: https://github.com/mozilla-mobile/fenix/issues/25170")
    fun `Updating SearchFragmentState from SearchState`() = runTest {
        val store = SearchFragmentStore(
            emptyDefaultState(
                searchEngineSource = SearchEngineSource.None,
                areShortcutsAvailable = false,
                defaultEngine = null,
                showSearchShortcutsSetting = true
            )
        )

        assertNull(store.state.defaultEngine)
        assertFalse(store.state.areShortcutsAvailable)
        assertFalse(store.state.showSearchShortcuts)
        assertEquals(SearchEngineSource.None, store.state.searchEngineSource)

        store.dispatch(
            SearchFragmentAction.UpdateSearchState(
                SearchState(
                    region = RegionState("US", "US"),
                    regionSearchEngines = listOf(
                        SearchEngine("engine-a", "Engine A", mockk(), type = SearchEngine.Type.BUNDLED),
                        SearchEngine("engine-b", "Engine B", mockk(), type = SearchEngine.Type.BUNDLED),
                        SearchEngine("engine-c", "Engine C", mockk(), type = SearchEngine.Type.BUNDLED)
                    ),
                    customSearchEngines = listOf(
                        SearchEngine("engine-d", "Engine D", mockk(), type = SearchEngine.Type.CUSTOM),
                        SearchEngine("engine-e", "Engine E", mockk(), type = SearchEngine.Type.CUSTOM)
                    ),
                    additionalSearchEngines = listOf(
                        SearchEngine("engine-f", "Engine F", mockk(), type = SearchEngine.Type.BUNDLED_ADDITIONAL)
                    ),
                    additionalAvailableSearchEngines = listOf(
                        SearchEngine("engine-g", "Engine G", mockk(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                        SearchEngine("engine-h", "Engine H", mockk(), type = SearchEngine.Type.BUNDLED_ADDITIONAL)
                    ),
                    hiddenSearchEngines = listOf(
                        SearchEngine("engine-i", "Engine I", mockk(), type = SearchEngine.Type.BUNDLED)
                    ),
                    regionDefaultSearchEngineId = "engine-b",
                    userSelectedSearchEngineId = null,
                    userSelectedSearchEngineName = null
                )
            )
        ).join()

        assertNotNull(store.state.defaultEngine)
        assertEquals("Engine B", store.state.defaultEngine!!.name)

        assertTrue(store.state.areShortcutsAvailable)
        assertTrue(store.state.showSearchShortcuts)

        assertTrue(store.state.searchEngineSource is SearchEngineSource.Default)
        assertNotNull(store.state.searchEngineSource.searchEngine)
        assertEquals("Engine B", store.state.searchEngineSource.searchEngine!!.name)
    }

    @Test
    @Ignore("Flaky, needs investigation: https://github.com/mozilla-mobile/fenix/issues/25170")
    fun `Updating SearchFragmentState from SearchState - shortcuts disabled`() = runTest {
        val store = SearchFragmentStore(
            emptyDefaultState(
                searchEngineSource = SearchEngineSource.None,
                areShortcutsAvailable = false,
                defaultEngine = null,
                showSearchShortcutsSetting = false
            )
        )

        assertNull(store.state.defaultEngine)
        assertFalse(store.state.areShortcutsAvailable)
        assertFalse(store.state.showSearchShortcuts)
        assertEquals(SearchEngineSource.None, store.state.searchEngineSource)

        store.dispatch(
            SearchFragmentAction.UpdateSearchState(
                SearchState(
                    region = RegionState("US", "US"),
                    regionSearchEngines = listOf(
                        SearchEngine("engine-a", "Engine A", mockk(), type = SearchEngine.Type.BUNDLED),
                        SearchEngine("engine-b", "Engine B", mockk(), type = SearchEngine.Type.BUNDLED),
                        SearchEngine("engine-c", "Engine C", mockk(), type = SearchEngine.Type.BUNDLED)
                    ),
                    customSearchEngines = listOf(
                        SearchEngine("engine-d", "Engine D", mockk(), type = SearchEngine.Type.CUSTOM),
                        SearchEngine("engine-e", "Engine E", mockk(), type = SearchEngine.Type.CUSTOM)
                    ),
                    additionalSearchEngines = listOf(
                        SearchEngine("engine-f", "Engine F", mockk(), type = SearchEngine.Type.BUNDLED_ADDITIONAL)
                    ),
                    additionalAvailableSearchEngines = listOf(
                        SearchEngine("engine-g", "Engine G", mockk(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                        SearchEngine("engine-h", "Engine H", mockk(), type = SearchEngine.Type.BUNDLED_ADDITIONAL)
                    ),
                    hiddenSearchEngines = listOf(
                        SearchEngine("engine-i", "Engine I", mockk(), type = SearchEngine.Type.BUNDLED)
                    ),
                    regionDefaultSearchEngineId = "engine-b",
                    userSelectedSearchEngineId = null,
                    userSelectedSearchEngineName = null
                )
            )
        ).join()

        assertNotNull(store.state.defaultEngine)
        assertEquals("Engine B", store.state.defaultEngine!!.name)

        assertTrue(store.state.areShortcutsAvailable)
        assertFalse(store.state.showSearchShortcuts)

        assertTrue(store.state.searchEngineSource is SearchEngineSource.Default)
        assertNotNull(store.state.searchEngineSource.searchEngine)
        assertEquals("Engine B", store.state.searchEngineSource.searchEngine!!.name)
    }

    private fun emptyDefaultState(
        searchEngineSource: SearchEngineSource = mockk(),
        defaultEngine: SearchEngine? = mockk(),
        areShortcutsAvailable: Boolean = true,
        showSearchShortcutsSetting: Boolean = false
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
        showHistorySuggestions = false,
        showBookmarkSuggestions = false,
        showSyncedTabsSuggestions = false,
        showSessionSuggestions = false,
        searchAccessPoint = MetricsUtils.Source.NONE
    )
}
