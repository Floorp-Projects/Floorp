/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.search.SearchEngine as LegacySearchEngine
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.search.ext.toDefaultSearchEngineProvider
import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.state.availableSearchEngines
import mozilla.components.browser.state.state.searchEngines
import mozilla.components.feature.search.ext.createSearchEngine
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class SearchUseCasesTest {

    private lateinit var searchEngine: LegacySearchEngine
    private lateinit var searchEngineManager: SearchEngineManager
    private lateinit var sessionManager: SessionManager
    private lateinit var store: BrowserStore
    private lateinit var useCases: SearchUseCases

    @Before
    fun setup() {
        searchEngine = mock()
        searchEngineManager = mock()
        sessionManager = mock()
        store = mock()
        useCases = SearchUseCases(
            store,
            searchEngineManager.toDefaultSearchEngineProvider(testContext),
            sessionManager
        )
    }

    @Test
    fun defaultSearch() {
        val searchTerms = "mozilla android"
        val searchUrl = "http://search-url.com?$searchTerms"
        val session = Session("mozilla.org")

        whenever(searchEngine.buildSearchUrl(searchTerms)).thenReturn(searchUrl)
        whenever(searchEngineManager.getDefaultSearchEngine(testContext)).thenReturn(searchEngine)

        useCases.defaultSearch(searchTerms, session)
        assertEquals(searchTerms, session.searchTerms)
        val actionCaptor = argumentCaptor<EngineAction.LoadUrlAction>()
        verify(store).dispatch(actionCaptor.capture())
        assertEquals(searchUrl, actionCaptor.value.url)
    }

    @Test
    fun defaultSearchOnNewSession() {
        val searchTerms = "mozilla android"
        val searchUrl = "http://search-url.com?$searchTerms"

        whenever(searchEngine.buildSearchUrl(searchTerms)).thenReturn(searchUrl)
        whenever(searchEngineManager.getDefaultSearchEngine(testContext)).thenReturn(searchEngine)

        useCases.newTabSearch(searchTerms, SessionState.Source.NEW_TAB)
        val actionCaptor = argumentCaptor<EngineAction.LoadUrlAction>()
        verify(store).dispatch(actionCaptor.capture())
        assertEquals(searchUrl, actionCaptor.value.url)
    }

    @Test
    fun `DefaultSearchUseCase invokes onNoSession if no session is selected`() {
        var createdSession: Session? = null

        whenever(searchEngine.buildSearchUrl("test")).thenReturn("https://search.example.com")
        whenever(searchEngineManager.getDefaultSearchEngine(testContext)).thenReturn(searchEngine)

        var sessionCreatedForUrl: String? = null

        val searchUseCases = SearchUseCases(
            store,
            searchEngineManager.toDefaultSearchEngineProvider(testContext),
            sessionManager
        ) { url ->
            sessionCreatedForUrl = url
            Session(url).also { createdSession = it }
        }

        searchUseCases.defaultSearch("test")
        assertEquals("https://search.example.com", sessionCreatedForUrl)
        assertNotNull(createdSession!!)
        verify(store).dispatch(EngineAction.LoadUrlAction(createdSession!!.id, sessionCreatedForUrl!!))
    }

    @Test
    fun newPrivateTabSearch() {
        val searchTerms = "mozilla android"
        val searchUrl = "http://search-url.com?$searchTerms"

        whenever(searchEngine.buildSearchUrl(searchTerms)).thenReturn(searchUrl)
        whenever(searchEngineManager.getDefaultSearchEngine(testContext)).thenReturn(searchEngine)
        useCases.newPrivateTabSearch.invoke(searchTerms)

        val captor = argumentCaptor<Session>()
        verify(sessionManager).add(captor.capture(), eq(true), eq(null), eq(null), eq(null))
        assertTrue(captor.value.private)
        val actionCaptor = argumentCaptor<EngineAction.LoadUrlAction>()
        verify(store).dispatch(actionCaptor.capture())
        assertEquals(searchUrl, actionCaptor.value.url)
    }

    @Test
    fun newPrivateTabSearchWithParentSession() {
        val searchTerms = "mozilla android"
        val searchUrl = "http://search-url.com?$searchTerms"

        whenever(searchEngine.buildSearchUrl(searchTerms)).thenReturn(searchUrl)
        whenever(searchEngineManager.getDefaultSearchEngine(testContext)).thenReturn(searchEngine)

        val parentSession = mock<Session>()
        whenever(sessionManager.findSessionById("test-parent")).thenReturn(parentSession)

        useCases.newPrivateTabSearch.invoke(searchTerms, parentSessionId = "test-parent")

        val captor = argumentCaptor<Session>()
        verify(sessionManager).add(captor.capture(), eq(true), eq(null), eq(null), eq(parentSession))
        assertTrue(captor.value.private)

        val actionCaptor = argumentCaptor<EngineAction.LoadUrlAction>()
        verify(store).dispatch(actionCaptor.capture())
        assertEquals(searchUrl, actionCaptor.value.url)
    }

    @Test
    fun `Selecting search engine`() {
        val store = BrowserStore(BrowserState(search = SearchState(
            region = RegionState("US", "US"),
            regionSearchEngines = listOf(
                SearchEngine("engine-a", "Engine A", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-b", "Engine B", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-c", "Engine C", mock(), type = SearchEngine.Type.BUNDLED)
            ),
            customSearchEngines = listOf(
                SearchEngine("engine-d", "Engine D", mock(), type = SearchEngine.Type.CUSTOM),
                SearchEngine("engine-e", "Engine E", mock(), type = SearchEngine.Type.CUSTOM)
            ),
            additionalSearchEngines = listOf(
                SearchEngine("engine-f", "Engine F", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL)
            ),
            additionalAvailableSearchEngines = listOf(
                SearchEngine("engine-g", "Engine G", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                SearchEngine("engine-h", "Engine H", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL)
            ),
            hiddenSearchEngines = listOf(
                SearchEngine("engine-i", "Engine I", mock(), type = SearchEngine.Type.BUNDLED)
            ),
            regionDefaultSearchEngineId = "engine-b",
            userSelectedSearchEngineId = null,
            userSelectedSearchEngineName = null
        )))

        val useCases = SearchUseCases(store, mock(), mock())

        useCases.selectSearchEngine.invoke(
            store.findSearchEngineById("engine-d")
        )

        store.waitUntilIdle()

        assertEquals("engine-d", store.state.search.userSelectedSearchEngineId)
        assertNull(store.state.search.userSelectedSearchEngineName)

        useCases.selectSearchEngine.invoke(
            store.findSearchEngineById("engine-b")
        )

        store.waitUntilIdle()

        assertEquals("engine-b", store.state.search.userSelectedSearchEngineId)
        assertEquals("Engine B", store.state.search.userSelectedSearchEngineName)

        useCases.selectSearchEngine.invoke(
            store.findSearchEngineById("engine-f")
        )

        store.waitUntilIdle()

        assertEquals("engine-f", store.state.search.userSelectedSearchEngineId)
        assertNull(store.state.search.userSelectedSearchEngineName)
    }

    @Test
    fun `addSearchEngine - add bundled engine`() {
        val store = BrowserStore(BrowserState(search = SearchState(
            region = RegionState("US", "US"),
            regionSearchEngines = listOf(
                SearchEngine("engine-a", "Engine A", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-b", "Engine B", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-c", "Engine C", mock(), type = SearchEngine.Type.BUNDLED)
            ),
            customSearchEngines = listOf(
                SearchEngine("engine-d", "Engine D", mock(), type = SearchEngine.Type.CUSTOM),
                SearchEngine("engine-e", "Engine E", mock(), type = SearchEngine.Type.CUSTOM)
            ),
            additionalSearchEngines = listOf(
                SearchEngine("engine-f", "Engine F", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL)
            ),
            additionalAvailableSearchEngines = listOf(
                SearchEngine("engine-g", "Engine G", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                SearchEngine("engine-h", "Engine H", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL)
            ),
            hiddenSearchEngines = listOf(
                SearchEngine("engine-i", "Engine I", mock(), type = SearchEngine.Type.BUNDLED)
            ),
            regionDefaultSearchEngineId = "engine-b",
            userSelectedSearchEngineId = null,
            userSelectedSearchEngineName = null
        )))

        val useCases = SearchUseCases(store, mock(), mock())

        assertEquals(6, store.state.search.searchEngines.size)
        assertEquals(3, store.state.search.availableSearchEngines.size)

        useCases.addSearchEngine.invoke(
            store.findSearchEngineById("engine-i")
        )

        store.waitUntilIdle()

        assertEquals(7, store.state.search.searchEngines.size)
        assertEquals(2, store.state.search.availableSearchEngines.size)

        assertEquals(4, store.state.search.regionSearchEngines.size)
        assertEquals(0, store.state.search.hiddenSearchEngines.size)

        assertEquals("engine-i", store.state.search.regionSearchEngines[3].id)
        assertEquals("Engine I", store.state.search.regionSearchEngines[3].name)
    }

    @Test
    fun `addSearchEngine - add additional bundled engine`() {
        val store = BrowserStore(BrowserState(search = SearchState(
            region = RegionState("US", "US"),
            regionSearchEngines = listOf(
                SearchEngine("engine-a", "Engine A", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-b", "Engine B", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-c", "Engine C", mock(), type = SearchEngine.Type.BUNDLED)
            ),
            customSearchEngines = listOf(
                SearchEngine("engine-d", "Engine D", mock(), type = SearchEngine.Type.CUSTOM),
                SearchEngine("engine-e", "Engine E", mock(), type = SearchEngine.Type.CUSTOM)
            ),
            additionalSearchEngines = listOf(
                SearchEngine("engine-f", "Engine F", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL)
            ),
            additionalAvailableSearchEngines = listOf(
                SearchEngine("engine-g", "Engine G", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                SearchEngine("engine-h", "Engine H", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL)
            ),
            hiddenSearchEngines = listOf(
                SearchEngine("engine-i", "Engine I", mock(), type = SearchEngine.Type.BUNDLED)
            ),
            regionDefaultSearchEngineId = "engine-b",
            userSelectedSearchEngineId = null,
            userSelectedSearchEngineName = null
        )))

        val useCases = SearchUseCases(store, mock(), mock())

        assertEquals(6, store.state.search.searchEngines.size)
        assertEquals(3, store.state.search.availableSearchEngines.size)

        useCases.addSearchEngine.invoke(
            store.findSearchEngineById("engine-h")
        )

        store.waitUntilIdle()

        assertEquals(7, store.state.search.searchEngines.size)
        assertEquals(2, store.state.search.availableSearchEngines.size)

        assertEquals(1, store.state.search.additionalAvailableSearchEngines.size)
        assertEquals(2, store.state.search.additionalSearchEngines.size)

        assertEquals("engine-h", store.state.search.additionalSearchEngines[1].id)
        assertEquals("Engine H", store.state.search.additionalSearchEngines[1].name)
    }

    @Test
    fun `addSearchEngine - add custom engine`() {
        val store = BrowserStore(BrowserState(search = SearchState(
            region = RegionState("US", "US"),
            regionSearchEngines = listOf(
                SearchEngine("engine-a", "Engine A", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-b", "Engine B", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-c", "Engine C", mock(), type = SearchEngine.Type.BUNDLED)
            ),
            customSearchEngines = listOf(
                SearchEngine("engine-d", "Engine D", mock(), type = SearchEngine.Type.CUSTOM),
                SearchEngine("engine-e", "Engine E", mock(), type = SearchEngine.Type.CUSTOM)
            ),
            additionalSearchEngines = listOf(
                SearchEngine("engine-f", "Engine F", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL)
            ),
            additionalAvailableSearchEngines = listOf(
                SearchEngine("engine-g", "Engine G", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                SearchEngine("engine-h", "Engine H", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL)
            ),
            hiddenSearchEngines = listOf(
                SearchEngine("engine-i", "Engine I", mock(), type = SearchEngine.Type.BUNDLED)
            ),
            regionDefaultSearchEngineId = "engine-b",
            userSelectedSearchEngineId = null,
            userSelectedSearchEngineName = null
        )))

        val useCases = SearchUseCases(store, mock(), mock())

        assertEquals(6, store.state.search.searchEngines.size)
        assertEquals(3, store.state.search.availableSearchEngines.size)

        useCases.addSearchEngine.invoke(
            createSearchEngine(
                name = "Engine X",
                url = "https://www.example.org/?q={searchTerms}",
                icon = mock()
            )
        )

        store.waitUntilIdle()

        assertEquals(7, store.state.search.searchEngines.size)
        assertEquals(3, store.state.search.availableSearchEngines.size)

        assertEquals(3, store.state.search.customSearchEngines.size)
        assertEquals("Engine X", store.state.search.customSearchEngines[2].name)
        assertEquals(
            "https://www.example.org/?q={searchTerms}",
            store.state.search.customSearchEngines[2].resultUrls[0]
        )
    }

    @Test
    fun `removeSearchEngine - remove bundled engine`() {
        val store = BrowserStore(BrowserState(search = SearchState(
            region = RegionState("US", "US"),
            regionSearchEngines = listOf(
                SearchEngine("engine-a", "Engine A", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-b", "Engine B", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-c", "Engine C", mock(), type = SearchEngine.Type.BUNDLED)
            ),
            customSearchEngines = listOf(
                SearchEngine("engine-d", "Engine D", mock(), type = SearchEngine.Type.CUSTOM),
                SearchEngine("engine-e", "Engine E", mock(), type = SearchEngine.Type.CUSTOM)
            ),
            additionalSearchEngines = listOf(
                SearchEngine("engine-f", "Engine F", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL)
            ),
            additionalAvailableSearchEngines = listOf(
                SearchEngine("engine-g", "Engine G", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                SearchEngine("engine-h", "Engine H", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL)
            ),
            hiddenSearchEngines = listOf(
                SearchEngine("engine-i", "Engine I", mock(), type = SearchEngine.Type.BUNDLED)
            ),
            regionDefaultSearchEngineId = "engine-b",
            userSelectedSearchEngineId = null,
            userSelectedSearchEngineName = null
        )))

        val useCases = SearchUseCases(store, mock(), mock())

        assertEquals(6, store.state.search.searchEngines.size)
        assertEquals(3, store.state.search.availableSearchEngines.size)

        useCases.removeSearchEngine.invoke(
            store.findSearchEngineById("engine-b")
        )

        store.waitUntilIdle()

        assertEquals(5, store.state.search.searchEngines.size)
        assertEquals(4, store.state.search.availableSearchEngines.size)

        assertEquals(2, store.state.search.regionSearchEngines.size)
        assertEquals(2, store.state.search.hiddenSearchEngines.size)

        assertEquals("engine-b", store.state.search.hiddenSearchEngines[1].id)
        assertEquals("Engine B", store.state.search.hiddenSearchEngines[1].name)
    }

    @Test
    fun `removeSearchEngine - remove additional bundled engine`() {
        val store = BrowserStore(BrowserState(search = SearchState(
            region = RegionState("US", "US"),
            regionSearchEngines = listOf(
                SearchEngine("engine-a", "Engine A", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-b", "Engine B", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-c", "Engine C", mock(), type = SearchEngine.Type.BUNDLED)
            ),
            customSearchEngines = listOf(
                SearchEngine("engine-d", "Engine D", mock(), type = SearchEngine.Type.CUSTOM),
                SearchEngine("engine-e", "Engine E", mock(), type = SearchEngine.Type.CUSTOM)
            ),
            additionalSearchEngines = listOf(
                SearchEngine("engine-f", "Engine F", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL)
            ),
            additionalAvailableSearchEngines = listOf(
                SearchEngine("engine-g", "Engine G", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                SearchEngine("engine-h", "Engine H", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL)
            ),
            hiddenSearchEngines = listOf(
                SearchEngine("engine-i", "Engine I", mock(), type = SearchEngine.Type.BUNDLED)
            ),
            regionDefaultSearchEngineId = "engine-b",
            userSelectedSearchEngineId = null,
            userSelectedSearchEngineName = null
        )))

        val useCases = SearchUseCases(store, mock(), mock())

        assertEquals(6, store.state.search.searchEngines.size)
        assertEquals(3, store.state.search.availableSearchEngines.size)

        useCases.removeSearchEngine.invoke(
            store.findSearchEngineById("engine-f")
        )

        store.waitUntilIdle()

        assertEquals(5, store.state.search.searchEngines.size)
        assertEquals(4, store.state.search.availableSearchEngines.size)

        assertEquals(0, store.state.search.additionalSearchEngines.size)
        assertEquals(3, store.state.search.additionalAvailableSearchEngines.size)

        assertEquals("engine-f", store.state.search.additionalAvailableSearchEngines[2].id)
        assertEquals("Engine F", store.state.search.additionalAvailableSearchEngines[2].name)
    }

    @Test
    fun `removeSearchEngine - remove custom engine`() {
        val store = BrowserStore(BrowserState(search = SearchState(
            region = RegionState("US", "US"),
            regionSearchEngines = listOf(
                SearchEngine("engine-a", "Engine A", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-b", "Engine B", mock(), type = SearchEngine.Type.BUNDLED),
                SearchEngine("engine-c", "Engine C", mock(), type = SearchEngine.Type.BUNDLED)
            ),
            customSearchEngines = listOf(
                SearchEngine("engine-d", "Engine D", mock(), type = SearchEngine.Type.CUSTOM),
                SearchEngine("engine-e", "Engine E", mock(), type = SearchEngine.Type.CUSTOM)
            ),
            additionalSearchEngines = listOf(
                SearchEngine("engine-f", "Engine F", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL)
            ),
            additionalAvailableSearchEngines = listOf(
                SearchEngine("engine-g", "Engine G", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL),
                SearchEngine("engine-h", "Engine H", mock(), type = SearchEngine.Type.BUNDLED_ADDITIONAL)
            ),
            hiddenSearchEngines = listOf(
                SearchEngine("engine-i", "Engine I", mock(), type = SearchEngine.Type.BUNDLED)
            ),
            regionDefaultSearchEngineId = "engine-b",
            userSelectedSearchEngineId = null,
            userSelectedSearchEngineName = null
        )))

        val useCases = SearchUseCases(store, mock(), mock())

        assertEquals(6, store.state.search.searchEngines.size)
        assertEquals(3, store.state.search.availableSearchEngines.size)

        useCases.removeSearchEngine.invoke(
            store.findSearchEngineById("engine-d")
        )

        store.waitUntilIdle()

        assertEquals(5, store.state.search.searchEngines.size)
        assertEquals(3, store.state.search.availableSearchEngines.size)

        assertEquals(1, store.state.search.customSearchEngines.size)
    }
}

private fun BrowserStore.findSearchEngineById(id: String): SearchEngine {
    val searchEngine = (state.search.searchEngines + state.search.availableSearchEngines).find {
        it.id == id
    }
    return requireNotNull(searchEngine)
}
