/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class SearchStateTest {
    @Test
    fun `selectedOrDefaultSearchEngine - selects engine by user selected search engine id`() {
        val state = SearchState(
            region = RegionState("US", "US"),
            regionSearchEngines = listOf(
                SearchEngine("engine-a", "Engine A", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-b", "Engine B", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-c", "Engine C", mock(), type = SearchEngine.Type.BUNDLED),
            ),
            customSearchEngines = listOf(
                SearchEngine("engine-d", "Engine D", mock(), type = SearchEngine.Type.CUSTOM),
                SearchEngine("engine-e", "Engine E", mock(), type = SearchEngine.Type.CUSTOM),
            ),
            applicationSearchEngines = listOf(
                SearchEngine("engine-j", "Engine J", mock(), type = SearchEngine.Type.APPLICATION),
            ),
            additionalSearchEngines = listOf(
                SearchEngine("engine-f", "Engine F", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
            ),
            additionalAvailableSearchEngines = listOf(
                SearchEngine("engine-g", "Engine G", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                SearchEngine("engine-h", "Engine H", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
            ),
            hiddenSearchEngines = listOf(
                SearchEngine("engine-i", "Engine I", mock(), type = SearchEngine.Type.BUNDLED),
            ),
            regionDefaultSearchEngineId = "engine-b",
            userSelectedSearchEngineId = "engine-e",
            userSelectedSearchEngineName = "Engine H", // Purposefully using name of other engine here
        )

        val searchEngine = state.selectedOrDefaultSearchEngine
        assertNotNull(searchEngine!!)
        assertEquals("engine-e", searchEngine.id)
        assertEquals("Engine E", searchEngine.name)
    }

    @Test
    fun `selectedOrDefaultSearchEngine - selects engine by user selected search engine name`() {
        val state = SearchState(
            region = RegionState("US", "US"),
            regionSearchEngines = listOf(
                SearchEngine("engine-a", "Engine A", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-b", "Engine B", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-c", "Engine C", mock(), type = SearchEngine.Type.BUNDLED),
            ),
            customSearchEngines = listOf(
                SearchEngine("engine-d", "Engine D", mock(), type = SearchEngine.Type.CUSTOM),
                SearchEngine("engine-e", "Engine E", mock(), type = SearchEngine.Type.CUSTOM),
            ),
            applicationSearchEngines = listOf(
                SearchEngine("engine-j", "Engine J", mock(), type = SearchEngine.Type.APPLICATION),
            ),
            additionalSearchEngines = listOf(
                SearchEngine("engine-f", "Engine F", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
            ),
            additionalAvailableSearchEngines = listOf(
                SearchEngine("engine-g", "Engine G", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                SearchEngine("engine-h", "Engine H", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
            ),
            hiddenSearchEngines = listOf(
                SearchEngine("engine-i", "Engine I", mock(), type = SearchEngine.Type.BUNDLED),
            ),
            regionDefaultSearchEngineId = "engine-b",
            userSelectedSearchEngineId = "engine-x",
            userSelectedSearchEngineName = "Engine D",
        )

        val searchEngine = state.selectedOrDefaultSearchEngine
        assertNotNull(searchEngine!!)
        assertEquals("engine-d", searchEngine.id)
        assertEquals("Engine D", searchEngine.name)
    }

    @Test
    fun `selectedOrDefaultSearchEngine - uses region default if user has made no choice`() {
        val state = SearchState(
            region = RegionState("US", "US"),
            regionSearchEngines = listOf(
                SearchEngine("engine-a", "Engine A", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-b", "Engine B", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-c", "Engine C", mock(), type = SearchEngine.Type.BUNDLED),
            ),
            customSearchEngines = listOf(
                SearchEngine("engine-d", "Engine D", mock(), type = SearchEngine.Type.CUSTOM),
                SearchEngine("engine-e", "Engine E", mock(), type = SearchEngine.Type.CUSTOM),
            ),
            applicationSearchEngines = listOf(
                SearchEngine("engine-j", "Engine J", mock(), type = SearchEngine.Type.APPLICATION),
            ),
            additionalSearchEngines = listOf(
                SearchEngine("engine-f", "Engine F", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
            ),
            additionalAvailableSearchEngines = listOf(
                SearchEngine("engine-g", "Engine G", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                SearchEngine("engine-h", "Engine H", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
            ),
            hiddenSearchEngines = listOf(
                SearchEngine("engine-i", "Engine I", mock(), type = SearchEngine.Type.BUNDLED),
            ),
            regionDefaultSearchEngineId = "engine-b",
            userSelectedSearchEngineId = null,
            userSelectedSearchEngineName = null,
        )

        val searchEngine = state.selectedOrDefaultSearchEngine
        assertNotNull(searchEngine!!)
        assertEquals("engine-b", searchEngine.id)
        assertEquals("Engine B", searchEngine.name)
    }

    @Test
    fun `selectedOrDefaultSearchEngine - fallback - use first region engine`() {
        val state = SearchState(
            region = RegionState("US", "US"),
            regionSearchEngines = listOf(
                SearchEngine("engine-a", "Engine A", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-b", "Engine B", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-c", "Engine C", mock(), type = SearchEngine.Type.BUNDLED),
            ),
            customSearchEngines = listOf(
                SearchEngine("engine-d", "Engine D", mock(), type = SearchEngine.Type.CUSTOM),
                SearchEngine("engine-e", "Engine E", mock(), type = SearchEngine.Type.CUSTOM),
            ),
            applicationSearchEngines = listOf(
                SearchEngine("engine-j", "Engine J", mock(), type = SearchEngine.Type.APPLICATION),
            ),
            additionalSearchEngines = listOf(
                SearchEngine("engine-f", "Engine F", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
            ),
            additionalAvailableSearchEngines = listOf(
                SearchEngine("engine-g", "Engine G", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                SearchEngine("engine-h", "Engine H", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
            ),
            hiddenSearchEngines = listOf(
                SearchEngine("engine-i", "Engine I", mock(), type = SearchEngine.Type.BUNDLED),
            ),
            regionDefaultSearchEngineId = null,
            userSelectedSearchEngineId = null,
            userSelectedSearchEngineName = null,
        )

        val searchEngine = state.selectedOrDefaultSearchEngine
        assertNotNull(searchEngine!!)
        assertEquals("engine-a", searchEngine.id)
        assertEquals("Engine A", searchEngine.name)
    }

    @Test
    fun `selectedOrDefaultSearchEngine - is null by default`() {
        val state = SearchState()
        assertNull(state.selectedOrDefaultSearchEngine)
    }

    @Test
    fun `searchEngines - combines lists`() {
        val state = SearchState(
            region = RegionState("US", "US"),
            regionSearchEngines = listOf(
                SearchEngine("engine-a", "Engine A", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-b", "Engine B", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-c", "Engine C", mock(), type = SearchEngine.Type.BUNDLED),
            ),
            customSearchEngines = listOf(
                SearchEngine("engine-d", "Engine D", mock(), type = SearchEngine.Type.CUSTOM),
                SearchEngine("engine-e", "Engine E", mock(), type = SearchEngine.Type.CUSTOM),
            ),
            applicationSearchEngines = listOf(
                SearchEngine("engine-j", "Engine J", mock(), type = SearchEngine.Type.APPLICATION),
            ),
            additionalSearchEngines = listOf(
                SearchEngine("engine-f", "Engine F", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
            ),
            additionalAvailableSearchEngines = listOf(
                SearchEngine("engine-g", "Engine G", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                SearchEngine("engine-h", "Engine H", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
            ),
            hiddenSearchEngines = listOf(
                SearchEngine("engine-i", "Engine I", mock(), type = SearchEngine.Type.BUNDLED),
            ),
            regionDefaultSearchEngineId = null,
            userSelectedSearchEngineId = null,
            userSelectedSearchEngineName = null,
        )

        val searchEngines = state.searchEngines
        assertEquals(7, searchEngines.size)
        assertEquals("engine-a", searchEngines[0].id)
        assertEquals("engine-b", searchEngines[1].id)
        assertEquals("engine-c", searchEngines[2].id)
        assertEquals("engine-f", searchEngines[3].id)
        assertEquals("engine-d", searchEngines[4].id)
        assertEquals("engine-e", searchEngines[5].id)
    }

    @Test
    fun `availableSearchEngines - combines lists`() {
        val state = SearchState(
            region = RegionState("US", "US"),
            regionSearchEngines = listOf(
                SearchEngine("engine-a", "Engine A", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-b", "Engine B", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-c", "Engine C", mock(), type = SearchEngine.Type.BUNDLED),
            ),
            customSearchEngines = listOf(
                SearchEngine("engine-d", "Engine D", mock(), type = SearchEngine.Type.CUSTOM),
                SearchEngine("engine-e", "Engine E", mock(), type = SearchEngine.Type.CUSTOM),
            ),
            applicationSearchEngines = listOf(
                SearchEngine("engine-j", "Engine J", mock(), type = SearchEngine.Type.APPLICATION),
            ),
            additionalSearchEngines = listOf(
                SearchEngine("engine-f", "Engine F", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
            ),
            additionalAvailableSearchEngines = listOf(
                SearchEngine("engine-g", "Engine G", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                SearchEngine("engine-h", "Engine H", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
            ),
            hiddenSearchEngines = listOf(
                SearchEngine("engine-i", "Engine I", mock(), type = SearchEngine.Type.BUNDLED),
            ),
            regionDefaultSearchEngineId = null,
            userSelectedSearchEngineId = null,
            userSelectedSearchEngineName = null,
        )

        val available = state.availableSearchEngines
        assertEquals(3, available.size)
        assertEquals("engine-i", available[0].id)
        assertEquals("engine-g", available[1].id)
        assertEquals("engine-h", available[2].id)
    }
}
