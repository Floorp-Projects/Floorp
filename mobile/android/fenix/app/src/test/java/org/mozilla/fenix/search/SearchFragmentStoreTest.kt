/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.provider.SearchEngineList
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.metrics.Event.PerformedSearch.SearchAccessPoint
import org.mozilla.fenix.components.searchengine.FenixSearchEngineProvider
import org.mozilla.fenix.utils.Settings

@ExperimentalCoroutinesApi
class SearchFragmentStoreTest {

    @MockK private lateinit var searchEngine: SearchEngine
    @MockK private lateinit var searchProvider: FenixSearchEngineProvider
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
        every { components.search.provider } returns searchProvider
        every { searchProvider.getDefaultEngine(activity) } returns searchEngine
        every { searchProvider.installedSearchEngines(activity) } returns SearchEngineList(
            list = listOf(mockk(), mockk()),
            default = searchEngine
        )
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
            searchEngineSource = SearchEngineSource.Default(searchEngine),
            defaultEngineSource = SearchEngineSource.Default(searchEngine),
            showSearchSuggestions = false,
            showSearchSuggestionsHint = false,
            showSearchShortcuts = true,
            areShortcutsAvailable = true,
            showClipboardSuggestions = false,
            showHistorySuggestions = false,
            showBookmarkSuggestions = false,
            tabId = null,
            pastedText = "pastedText",
            searchAccessPoint = SearchAccessPoint.ACTION
        )

        assertEquals(
            expected,
            createInitialSearchFragmentState(
                activity,
                components,
                tabId = null,
                pastedText = "pastedText",
                searchAccessPoint = SearchAccessPoint.ACTION
            )
        )
        assertEquals(
            expected.copy(tabId = "tabId"),
            createInitialSearchFragmentState(
                activity,
                components,
                tabId = "tabId",
                pastedText = "pastedText",
                searchAccessPoint = SearchAccessPoint.ACTION
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
                searchEngineSource = SearchEngineSource.Default(searchEngine),
                defaultEngineSource = SearchEngineSource.Default(searchEngine),
                showSearchSuggestions = false,
                showSearchSuggestionsHint = false,
                showSearchShortcuts = false,
                areShortcutsAvailable = true,
                showClipboardSuggestions = false,
                showHistorySuggestions = false,
                showBookmarkSuggestions = false,
                tabId = "tabId",
                pastedText = "",
                searchAccessPoint = SearchAccessPoint.SHORTCUT
            ),
            createInitialSearchFragmentState(
                activity,
                components,
                tabId = "tabId",
                pastedText = "",
                searchAccessPoint = SearchAccessPoint.SHORTCUT
            )
        )
    }

    @Test
    fun updateQuery() = runBlocking {
        val initialState = emptyDefaultState()
        val store = SearchFragmentStore(initialState)
        val query = "test query"

        store.dispatch(SearchFragmentAction.UpdateQuery(query)).join()
        assertNotSame(initialState, store.state)
        assertEquals(query, store.state.query)
    }

    @Test
    fun selectSearchShortcutEngine() = runBlocking {
        val initialState = emptyDefaultState()
        val store = SearchFragmentStore(initialState)

        store.dispatch(SearchFragmentAction.SearchShortcutEngineSelected(searchEngine)).join()
        assertNotSame(initialState, store.state)
        assertEquals(SearchEngineSource.Shortcut(searchEngine), store.state.searchEngineSource)
        assertEquals(false, store.state.showSearchShortcuts)
    }

    @Test
    fun showSearchShortcutEnginePicker() = runBlocking {
        val initialState = emptyDefaultState()
        val store = SearchFragmentStore(initialState)

        store.dispatch(SearchFragmentAction.ShowSearchShortcutEnginePicker(true)).join()
        assertNotSame(initialState, store.state)
        assertEquals(true, store.state.showSearchShortcuts)
    }

    @Test
    fun hideSearchShortcutEnginePicker() = runBlocking {
        val initialState = emptyDefaultState()
        val store = SearchFragmentStore(initialState)

        store.dispatch(SearchFragmentAction.UpdateShortcutsAvailability(false)).join()
        assertNotSame(initialState, store.state)
        assertEquals(false, store.state.showSearchShortcuts)
    }

    @Test
    fun showSearchSuggestions() = runBlocking {
        val initialState = emptyDefaultState()
        val store = SearchFragmentStore(initialState)

        store.dispatch(SearchFragmentAction.SetShowSearchSuggestions(true)).join()
        assertNotSame(initialState, store.state)
        assertTrue(store.state.showSearchSuggestions)

        store.dispatch(SearchFragmentAction.SetShowSearchSuggestions(false)).join()
        assertFalse(store.state.showSearchSuggestions)
    }

    @Test
    fun allowSearchInPrivateMode() = runBlocking {
        val initialState = emptyDefaultState()
        val store = SearchFragmentStore(initialState)

        store.dispatch(SearchFragmentAction.AllowSearchSuggestionsInPrivateModePrompt(true)).join()
        assertNotSame(initialState, store.state)
        assertTrue(store.state.showSearchSuggestionsHint)

        store.dispatch(SearchFragmentAction.AllowSearchSuggestionsInPrivateModePrompt(false)).join()
        assertFalse(store.state.showSearchSuggestionsHint)
    }

    @Test
    fun selectNewDefaultEngine() = runBlocking {
        val initialState = emptyDefaultState()
        val store = SearchFragmentStore(initialState)

        store.dispatch(SearchFragmentAction.SelectNewDefaultSearchEngine(searchEngine)).join()
        assertNotSame(initialState, store.state)
        assertEquals(SearchEngineSource.Default(searchEngine), store.state.searchEngineSource)
    }

    private fun emptyDefaultState(): SearchFragmentState = SearchFragmentState(
        tabId = null,
        url = "",
        searchTerms = "",
        query = "",
        searchEngineSource = mockk(),
        defaultEngineSource = mockk(),
        showSearchSuggestionsHint = false,
        showSearchSuggestions = false,
        showSearchShortcuts = false,
        areShortcutsAvailable = true,
        showClipboardSuggestions = false,
        showHistorySuggestions = false,
        showBookmarkSuggestions = false,
        searchAccessPoint = SearchAccessPoint.NONE
    )
}
