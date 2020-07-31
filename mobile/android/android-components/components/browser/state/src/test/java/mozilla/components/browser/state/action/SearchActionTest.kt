/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class SearchActionTest {
    @Test
    fun `AddSearchEngineListAction adds new searchEngines`() {
        val store = BrowserStore(BrowserState())
        val searchEngineList = listOf(
            SearchEngine(
                id = "id1",
                name = "search1",
                icon = mock(),
                type = SearchEngine.Type.BUNDLED
            ),
            SearchEngine(
                id = "id2",
                name = "search2",
                icon = mock(),
                type = SearchEngine.Type.BUNDLED
            )
        )
        assertTrue(store.state.search.searchEngines.isEmpty())

        store.dispatch(SearchAction.AddSearchEngineListAction(searchEngineList)).joinBlocking()
        val searchEnginesMap = store.state.search.searchEngines
        assertFalse(searchEnginesMap.isEmpty())
        assertNotNull(searchEnginesMap[searchEngineList[0].id])
        assertNotNull(searchEnginesMap[searchEngineList[1].id])
    }

    @Test
    fun `SetCustomSearchEngineAction sets a new custom search engine`() {
        val store = BrowserStore(BrowserState())
        val searchEngineList = listOf(
            SearchEngine(
                id = "id1",
                name = "search1",
                icon = mock(),
                type = SearchEngine.Type.BUNDLED
            )
        )
        store.dispatch(SearchAction.AddSearchEngineListAction(searchEngineList)).joinBlocking()

        val customSearchEngine = SearchEngine(
            id = "customId1",
            name = "custom_search",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM
        )
        store.dispatch(SearchAction.SetCustomSearchEngineAction(customSearchEngine)).joinBlocking()
        val searchEnginesMap = store.state.search.searchEngines
        assertEquals(2, searchEnginesMap.size)
        assertNotNull(searchEnginesMap[customSearchEngine.id])
    }

    @Test
    fun `RemoveCustomSearchEngineAction removes a new custom search engine`() {
        val store = BrowserStore(BrowserState())
        val customSearchEngine = SearchEngine(
            id = "customId1",
            name = "custom_search",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM
        )
        val searchEngineList = listOf(customSearchEngine)
        store.dispatch(SearchAction.AddSearchEngineListAction(searchEngineList)).joinBlocking()

        store.dispatch(SearchAction.RemoveCustomSearchEngineAction("unrecognized_id")).joinBlocking()
        assertFalse(store.state.search.searchEngines.isEmpty())

        store.dispatch(SearchAction.RemoveCustomSearchEngineAction(customSearchEngine.id)).joinBlocking()
        assertTrue(store.state.search.searchEngines.isEmpty())
    }

    @Test
    fun `SetDefaultSearchEngineAction sets a default search engine id`() {
        val store = BrowserStore(BrowserState())
        val searchEngine = SearchEngine(
            id = "id1",
            name = "search1",
            icon = mock(),
            type = SearchEngine.Type.BUNDLED
        )
        val searchEngineList = listOf(searchEngine)
        store.dispatch(SearchAction.AddSearchEngineListAction(searchEngineList)).joinBlocking()
        assertNull(store.state.search.defaultSearchEngineId)

        store.dispatch(SearchAction.SetDefaultSearchEngineAction(searchEngine.id)).joinBlocking()
        assertEquals(searchEngine.id, store.state.search.defaultSearchEngineId)

        store.dispatch(SearchAction.SetDefaultSearchEngineAction("unrecognized_id")).joinBlocking()
        assertEquals(searchEngine.id, store.state.search.defaultSearchEngineId)
    }
}
