/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import io.mockk.mockk
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.search.SearchEngine
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Test

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

    private fun emptyDefaultState(): SearchFragmentState = SearchFragmentState(
        query = "",
        searchEngineSource = mockk(),
        defaultEngineSource = mockk(),
        showSearchSuggestions = false,
        showSearchShortcuts = false,
        showClipboardSuggestions = false,
        showHistorySuggestions = false,
        showBookmarkSuggestions = false,
        session = null
    )
}
