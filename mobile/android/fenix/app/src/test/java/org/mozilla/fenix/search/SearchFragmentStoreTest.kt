/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import io.mockk.mockk
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.search.SearchEngine
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.components.metrics.Event

@ExperimentalCoroutinesApi
class SearchFragmentStoreTest {

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
        val searchEngine: SearchEngine = mockk()

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
        val engine = mockk<SearchEngine>()

        store.dispatch(SearchFragmentAction.SelectNewDefaultSearchEngine(engine)).join()
        assertNotSame(initialState, store.state)
        assertEquals(SearchEngineSource.Default(engine), store.state.searchEngineSource)
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
        searchAccessPoint = Event.PerformedSearch.SearchAccessPoint.NONE
    )
}
