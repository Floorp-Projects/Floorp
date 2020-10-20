/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SearchState
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
    fun `SetSearchEnginesAction - Set sets region search engines in state`() {
        val engine1 = SearchEngine(
            id = "id1",
            name = "search1",
            icon = mock(),
            type = SearchEngine.Type.BUNDLED
        )
        val engine2 = SearchEngine(
            id = "id2",
            name = "search2",
            icon = mock(),
            type = SearchEngine.Type.BUNDLED
        )

        val store = BrowserStore(BrowserState())
        val searchEngineList = listOf(engine1, engine2)
        assertTrue(store.state.search.regionSearchEngines.isEmpty())

        store.dispatch(SearchAction.SetSearchEnginesAction(
            regionSearchEngines = searchEngineList,
            regionDefaultSearchEngineId = "id2",
            customSearchEngines = emptyList(),
            defaultSearchEngineId = null
        )).joinBlocking()

        val searchEngines = store.state.search.regionSearchEngines
        assertFalse(searchEngines.isEmpty())
        assertEquals(2, searchEngines.size)
        assertEquals(engine1, searchEngines[0])
        assertEquals(engine2, searchEngines[1])
    }

    @Test
    fun `SetSearchEnginesAction - sets custom search engines in state`() {
        val engine1 = SearchEngine(
            id = "id1",
            name = "search1",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM
        )
        val engine2 = SearchEngine(
            id = "id2",
            name = "search2",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM
        )

        val store = BrowserStore(BrowserState())
        val searchEngineList = listOf(engine1, engine2)
        assertTrue(store.state.search.customSearchEngines.isEmpty())

        store.dispatch(SearchAction.SetSearchEnginesAction(
            customSearchEngines = searchEngineList,
            regionSearchEngines = emptyList(),
            regionDefaultSearchEngineId = "default",
            defaultSearchEngineId = null
        )).joinBlocking()

        val searchEngines = store.state.search.customSearchEngines
        assertFalse(searchEngines.isEmpty())
        assertEquals(2, searchEngines.size)
        assertEquals(engine1, searchEngines[0])
        assertEquals(engine2, searchEngines[1])
    }

    @Test
    fun `UpdateCustomSearchEngineAction sets a new custom search engine`() {
        val store = BrowserStore(BrowserState())

        assertTrue(store.state.search.customSearchEngines.isEmpty())

        val customSearchEngine = SearchEngine(
            id = "customId1",
            name = "custom_search",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM
        )

        // Add a custom search engine
        store.dispatch(SearchAction.UpdateCustomSearchEngineAction(customSearchEngine)).joinBlocking()

        store.state.search.customSearchEngines.let { searchEngines ->
            assertTrue(searchEngines.isNotEmpty())
            assertEquals(1, searchEngines.size)
            assertEquals(customSearchEngine, searchEngines[0])
        }

        val customSearchEngine2 = SearchEngine(
            id = "customId2",
            name = "custom_search_second",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM
        )

        // Add another search engine
        store.dispatch(SearchAction.UpdateCustomSearchEngineAction(customSearchEngine2)).joinBlocking()

        store.state.search.customSearchEngines.let { searchEngines ->
            assertTrue(searchEngines.isNotEmpty())
            assertEquals(2, searchEngines.size)
            assertEquals(customSearchEngine, searchEngines[0])
            assertEquals(customSearchEngine2, searchEngines[1])
        }

        // Update first search engine
        val updated = customSearchEngine.copy(
            name = "My awesome search engine"
        )
        store.dispatch(SearchAction.UpdateCustomSearchEngineAction(updated)).joinBlocking()

        store.state.search.customSearchEngines.let { searchEngines ->
            assertTrue(searchEngines.isNotEmpty())
            assertEquals(2, searchEngines.size)
            assertEquals(updated, searchEngines[0])
            assertEquals(customSearchEngine2, searchEngines[1])
        }
    }

    @Test
    fun `RemoveCustomSearchEngineAction removes a new custom search engine`() {
        val customSearchEngine = SearchEngine(
            id = "customId1",
            name = "custom_search",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM
        )

        val store = BrowserStore(
            BrowserState(
                search = SearchState(
                    customSearchEngines = listOf(customSearchEngine)
                )
            )
        )

        assertEquals(1, store.state.search.customSearchEngines.size)

        store.dispatch(SearchAction.RemoveCustomSearchEngineAction("unrecognized_id")).joinBlocking()
        assertEquals(1, store.state.search.customSearchEngines.size)

        store.dispatch(SearchAction.RemoveCustomSearchEngineAction(customSearchEngine.id)).joinBlocking()
        assertTrue(store.state.search.customSearchEngines.isEmpty())
    }

    @Test
    fun `SetDefaultSearchEngineAction sets a default search engine id`() {
        val searchEngine = SearchEngine(
            id = "id1",
            name = "search1",
            icon = mock(),
            type = SearchEngine.Type.BUNDLED
        )

        val store = BrowserStore(
            BrowserState(
                search = SearchState(
                    regionSearchEngines = listOf(searchEngine)
                )
            )
        )

        assertNull(store.state.search.defaultSearchEngineId)

        store.dispatch(SearchAction.SetDefaultSearchEngineAction(searchEngine.id)).joinBlocking()
        assertEquals(searchEngine.id, store.state.search.defaultSearchEngineId)

        assertEquals(searchEngine.id, store.state.search.defaultSearchEngineId)

        store.dispatch(SearchAction.SetDefaultSearchEngineAction("unrecognized_id")).joinBlocking()
        // We allow setting an ID of a search engine that is not in the state since loading happens
        // asynchronously and the search engine may not be loaded yet.
        assertEquals("unrecognized_id", store.state.search.defaultSearchEngineId)
    }

    @Test
    fun `Setting region of user`() {
        val store = BrowserStore()
        assertNull(store.state.search.region)

        store.dispatch(SearchAction.SetRegionAction(RegionState("DE", "FR"))).joinBlocking()

        assertNotNull(store.state.search.region)
        assertEquals("DE", store.state.search.region!!.home)
        assertEquals("FR", store.state.search.region!!.current)
    }
}
