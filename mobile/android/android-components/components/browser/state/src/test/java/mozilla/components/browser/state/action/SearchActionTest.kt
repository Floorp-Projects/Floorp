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
            type = SearchEngine.Type.BUNDLED,
        )
        val engine2 = SearchEngine(
            id = "id2",
            name = "search2",
            icon = mock(),
            type = SearchEngine.Type.BUNDLED,
        )

        val store = BrowserStore(BrowserState())
        val searchEngineList = listOf(engine1, engine2)
        assertTrue(store.state.search.regionSearchEngines.isEmpty())

        store.dispatch(
            SearchAction.SetSearchEnginesAction(
                regionSearchEngines = searchEngineList,
                regionDefaultSearchEngineId = "id2",
                customSearchEngines = emptyList(),
                userSelectedSearchEngineId = null,
                userSelectedSearchEngineName = null,
                hiddenSearchEngines = emptyList(),
                additionalSearchEngines = emptyList(),
                additionalAvailableSearchEngines = emptyList(),
                regionSearchEnginesOrder = listOf("id1", "id2"),
            ),
        ).joinBlocking()

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
            type = SearchEngine.Type.CUSTOM,
        )
        val engine2 = SearchEngine(
            id = "id2",
            name = "search2",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM,
        )

        val store = BrowserStore(BrowserState())
        val searchEngineList = listOf(engine1, engine2)
        assertTrue(store.state.search.customSearchEngines.isEmpty())

        store.dispatch(
            SearchAction.SetSearchEnginesAction(
                customSearchEngines = searchEngineList,
                regionSearchEngines = emptyList(),
                regionDefaultSearchEngineId = "default",
                userSelectedSearchEngineId = null,
                userSelectedSearchEngineName = null,
                hiddenSearchEngines = emptyList(),
                additionalSearchEngines = emptyList(),
                additionalAvailableSearchEngines = emptyList(),
                regionSearchEnginesOrder = emptyList(),
            ),
        ).joinBlocking()

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
            type = SearchEngine.Type.CUSTOM,
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
            type = SearchEngine.Type.CUSTOM,
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
            name = "My awesome search engine",
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
            type = SearchEngine.Type.CUSTOM,
        )

        val store = BrowserStore(
            BrowserState(
                search = SearchState(
                    customSearchEngines = listOf(customSearchEngine),
                ),
            ),
        )

        assertEquals(1, store.state.search.customSearchEngines.size)

        store.dispatch(SearchAction.RemoveCustomSearchEngineAction("unrecognized_id")).joinBlocking()
        assertEquals(1, store.state.search.customSearchEngines.size)

        store.dispatch(SearchAction.RemoveCustomSearchEngineAction(customSearchEngine.id)).joinBlocking()
        assertTrue(store.state.search.customSearchEngines.isEmpty())
    }

    @Test
    fun `SelectSearchEngineAction sets a default search engine id`() {
        val searchEngine = SearchEngine(
            id = "id1",
            name = "search1",
            icon = mock(),
            type = SearchEngine.Type.BUNDLED,
        )

        val store = BrowserStore(
            BrowserState(
                search = SearchState(
                    regionSearchEngines = listOf(searchEngine),
                ),
            ),
        )

        assertNull(store.state.search.userSelectedSearchEngineId)

        store.dispatch(SearchAction.SelectSearchEngineAction(searchEngine.id, null)).joinBlocking()
        assertEquals(searchEngine.id, store.state.search.userSelectedSearchEngineId)

        assertEquals(searchEngine.id, store.state.search.userSelectedSearchEngineId)

        store.dispatch(SearchAction.SelectSearchEngineAction("unrecognized_id", null)).joinBlocking()
        // We allow setting an ID of a search engine that is not in the state since loading happens
        // asynchronously and the search engine may not be loaded yet.
        assertEquals("unrecognized_id", store.state.search.userSelectedSearchEngineId)
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

    @Test
    fun `ShowSearchEngineAction - Adds hidden search engines back to region search engines`() {
        val store = BrowserStore(
            BrowserState(
                search = SearchState(
                    regionSearchEngines = listOf(
                        SearchEngine(id = "google", name = "Google", icon = mock(), type = SearchEngine.Type.BUNDLED),
                        SearchEngine(id = "bing", name = "Bing", icon = mock(), type = SearchEngine.Type.BUNDLED),
                    ),
                    hiddenSearchEngines = listOf(
                        SearchEngine(id = "duckduckgo", name = "DuckDuckGo", icon = mock(), type = SearchEngine.Type.BUNDLED),
                    ),
                ),
            ),
        )

        store.dispatch(
            SearchAction.ShowSearchEngineAction("duckduckgo"),
        ).joinBlocking()

        assertEquals(0, store.state.search.hiddenSearchEngines.size)
        assertEquals(3, store.state.search.regionSearchEngines.size)

        assertEquals("google", store.state.search.regionSearchEngines[0].id)
        assertEquals("bing", store.state.search.regionSearchEngines[1].id)
        assertEquals("duckduckgo", store.state.search.regionSearchEngines[2].id)
    }

    @Test
    fun `HideSearchEngineAction - Adds region search engine to hidden search engines`() {
        val store = BrowserStore(
            BrowserState(
                search = SearchState(
                    regionSearchEngines = listOf(
                        SearchEngine(id = "google", name = "Google", icon = mock(), type = SearchEngine.Type.BUNDLED),
                        SearchEngine(id = "bing", name = "Bing", icon = mock(), type = SearchEngine.Type.BUNDLED),
                    ),
                    hiddenSearchEngines = listOf(
                        SearchEngine(id = "duckduckgo", name = "DuckDuckGo", icon = mock(), type = SearchEngine.Type.BUNDLED),
                    ),
                ),
            ),
        )

        store.dispatch(
            SearchAction.HideSearchEngineAction("google"),
        ).joinBlocking()

        assertEquals(2, store.state.search.hiddenSearchEngines.size)
        assertEquals(1, store.state.search.regionSearchEngines.size)

        assertEquals("bing", store.state.search.regionSearchEngines[0].id)

        assertEquals("duckduckgo", store.state.search.hiddenSearchEngines[0].id)
        assertEquals("google", store.state.search.hiddenSearchEngines[1].id)
    }

    @Test
    fun `ShowSearchEngineAction, HideSearchEngineAction - Does nothing for unknown or custom search engines`() {
        val store = BrowserStore(
            BrowserState(
                search = SearchState(
                    regionSearchEngines = listOf(
                        SearchEngine(id = "google", name = "Google", icon = mock(), type = SearchEngine.Type.BUNDLED),
                        SearchEngine(id = "bing", name = "Bing", icon = mock(), type = SearchEngine.Type.BUNDLED),
                    ),
                    hiddenSearchEngines = listOf(
                        SearchEngine(id = "duckduckgo", name = "DuckDuckGo", icon = mock(), type = SearchEngine.Type.BUNDLED),
                    ),
                    customSearchEngines = listOf(
                        SearchEngine(id = "banana", name = "Banana Search", icon = mock(), type = SearchEngine.Type.CUSTOM),
                    ),
                ),
            ),
        )

        store.dispatch(
            SearchAction.ShowSearchEngineAction("banana"),
        ).joinBlocking()

        store.dispatch(
            SearchAction.HideSearchEngineAction("banana"),
        ).joinBlocking()

        store.dispatch(
            SearchAction.HideSearchEngineAction("unknown-search"),
        ).joinBlocking()

        store.dispatch(
            SearchAction.ShowSearchEngineAction("also-unknown-search"),
        ).joinBlocking()

        assertEquals(2, store.state.search.regionSearchEngines.size)
        assertEquals(1, store.state.search.hiddenSearchEngines.size)
        assertEquals(1, store.state.search.customSearchEngines.size)

        assertEquals("google", store.state.search.regionSearchEngines[0].id)
        assertEquals("bing", store.state.search.regionSearchEngines[1].id)

        assertEquals("duckduckgo", store.state.search.hiddenSearchEngines[0].id)

        assertEquals("banana", store.state.search.customSearchEngines[0].id)
    }
}
