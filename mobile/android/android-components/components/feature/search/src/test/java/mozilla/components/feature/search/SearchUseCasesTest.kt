/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.search.ext.toDefaultSearchEngineProvider
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.availableSearchEngines
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.state.searchEngines
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.search.ext.createSearchEngine
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import mozilla.components.browser.search.SearchEngine as LegacySearchEngine

@RunWith(AndroidJUnit4::class)
class SearchUseCasesTest {

    private lateinit var searchEngine: LegacySearchEngine
    private lateinit var searchEngineManager: SearchEngineManager
    private lateinit var store: BrowserStore
    private lateinit var useCases: SearchUseCases
    private lateinit var tabsUseCases: TabsUseCases

    private val middleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()

    @Before
    fun setup() {
        searchEngine = mock()
        searchEngineManager = mock()
        tabsUseCases = mock()

        store = BrowserStore(middleware = listOf(middleware))

        useCases = SearchUseCases(
            store,
            searchEngineManager.toDefaultSearchEngineProvider(testContext),
            tabsUseCases
        )
    }

    @After
    fun tearDown() {
        middleware.reset()
    }

    @Test
    fun defaultSearch() {
        val searchTerms = "mozilla android"
        val searchUrl = "http://search-url.com?$searchTerms"

        store.dispatch(TabListAction.AddTabAction(
            tab = createTab("https://www.mozilla.org", id = "mozilla"),
            select = true
        )).joinBlocking()

        whenever(searchEngine.buildSearchUrl(searchTerms)).thenReturn(searchUrl)
        whenever(searchEngineManager.getDefaultSearchEngine(testContext)).thenReturn(searchEngine)

        useCases.defaultSearch(searchTerms)

        store.waitUntilIdle()

        val loadUrlAction = middleware.findAction(EngineAction.LoadUrlAction::class)
        assertEquals(searchUrl, loadUrlAction.url)
    }

    @Test
    fun defaultSearchOnNewSession() {
        val searchTerms = "mozilla android"
        val searchUrl = "http://search-url.com?$searchTerms"

        whenever(searchEngine.buildSearchUrl(searchTerms)).thenReturn(searchUrl)
        whenever(searchEngineManager.getDefaultSearchEngine(testContext)).thenReturn(searchEngine)

        val newTabUseCase: TabsUseCases.AddNewTabUseCase = mock()
        whenever(tabsUseCases.addTab).thenReturn(newTabUseCase)
        whenever(newTabUseCase(searchUrl)).thenReturn("2342")

        useCases.newTabSearch(searchTerms, SessionState.Source.NEW_TAB)

        verify(newTabUseCase).invoke(
            searchUrl,
            parentId = null,
            selectTab = true,
            source = SessionState.Source.NEW_TAB
        )

        val searchTermsAction = middleware.findAction(ContentAction.UpdateSearchTermsAction::class)
        assertEquals("2342", searchTermsAction.sessionId)
        assertEquals(searchTerms, searchTermsAction.searchTerms)
    }

    @Test
    fun `DefaultSearchUseCase creates new tab if no session is selected`() {
        val searchTerms = "mozilla android"
        val searchUrl = "http://search-url.com?$searchTerms"

        whenever(searchEngine.buildSearchUrl(searchTerms)).thenReturn(searchUrl)
        whenever(searchEngineManager.getDefaultSearchEngine(testContext)).thenReturn(searchEngine)

        val newTabUseCase: TabsUseCases.AddNewTabUseCase = mock()
        whenever(tabsUseCases.addTab).thenReturn(newTabUseCase)
        whenever(newTabUseCase(searchUrl)).thenReturn("2342")

        useCases.defaultSearch(searchTerms)

        verify(newTabUseCase).invoke(
            searchUrl,
            parentId = null,
            selectTab = true,
            source = SessionState.Source.NEW_TAB
        )

        val searchTermsAction = middleware.findAction(ContentAction.UpdateSearchTermsAction::class)
        assertEquals("2342", searchTermsAction.sessionId)
        assertEquals(searchTerms, searchTermsAction.searchTerms)
    }

    @Test
    fun newPrivateTabSearch() {
        val searchTerms = "mozilla android"
        val searchUrl = "http://search-url.com?$searchTerms"

        whenever(searchEngine.buildSearchUrl(searchTerms)).thenReturn(searchUrl)
        whenever(searchEngineManager.getDefaultSearchEngine(testContext)).thenReturn(searchEngine)

        val newTabUseCase: TabsUseCases.AddNewPrivateTabUseCase = mock()
        whenever(tabsUseCases.addPrivateTab).thenReturn(newTabUseCase)
        whenever(newTabUseCase(searchUrl, source = SessionState.Source.NONE)).thenReturn("1177")

        useCases.newPrivateTabSearch.invoke(searchTerms)

        verify(newTabUseCase).invoke(
            searchUrl,
            parentId = null,
            selectTab = true,
            source = SessionState.Source.NONE
        )

        val searchTermsAction = middleware.findAction(ContentAction.UpdateSearchTermsAction::class)
        assertEquals("1177", searchTermsAction.sessionId)
        assertEquals(searchTerms, searchTermsAction.searchTerms)
    }

    @Test
    fun newPrivateTabSearchWithParentSession() {
        val searchTerms = "mozilla android"
        val searchUrl = "http://search-url.com?$searchTerms"

        whenever(searchEngine.buildSearchUrl(searchTerms)).thenReturn(searchUrl)
        whenever(searchEngineManager.getDefaultSearchEngine(testContext)).thenReturn(searchEngine)

        val newTabUseCase: TabsUseCases.AddNewPrivateTabUseCase = mock()
        whenever(tabsUseCases.addPrivateTab).thenReturn(newTabUseCase)
        whenever(newTabUseCase(searchUrl, source = SessionState.Source.NONE, parentId = "test-parent")).thenReturn("1177")

        useCases.newPrivateTabSearch.invoke(searchTerms, parentSessionId = "test-parent")

        verify(newTabUseCase).invoke(
            searchUrl,
            parentId = "test-parent",
            selectTab = true,
            source = SessionState.Source.NONE
        )

        val searchTermsAction = middleware.findAction(ContentAction.UpdateSearchTermsAction::class)
        assertEquals("1177", searchTermsAction.sessionId)
        assertEquals(searchTerms, searchTermsAction.searchTerms)
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
