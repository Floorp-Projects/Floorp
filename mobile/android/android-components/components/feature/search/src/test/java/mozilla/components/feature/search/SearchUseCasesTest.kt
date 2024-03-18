/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search

import androidx.test.ext.junit.runners.AndroidJUnit4
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
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.feature.search.ext.createSearchEngine
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class SearchUseCasesTest {

    private lateinit var searchEngine: SearchEngine
    private lateinit var store: BrowserStore
    private lateinit var useCases: SearchUseCases
    private lateinit var tabsUseCases: TabsUseCases
    private lateinit var sessionUseCases: SessionUseCases
    private lateinit var loadUrlUseCase: SessionUseCases.DefaultLoadUrlUseCase

    private val middleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()

    private val searchTerms = "mozilla android"
    private val searchUrl = "https://example.org/?q=mozilla%20android"
    private val searchEngineName = "Test"

    @Before
    fun setup() {
        searchEngine = createSearchEngine(
            name = searchEngineName,
            url = "https://example.org/?q={searchTerms}",
            icon = mock(),
        )

        tabsUseCases = mock()
        sessionUseCases = mock()
        loadUrlUseCase = mock()
        doReturn(loadUrlUseCase).`when`(sessionUseCases).loadUrl

        store = BrowserStore(
            initialState = BrowserState(
                search = SearchState(
                    regionSearchEngines = listOf(searchEngine),
                ),
            ),
            middleware = listOf(middleware),
        )

        useCases = SearchUseCases(
            store,
            tabsUseCases,
            sessionUseCases,
        )
    }

    @After
    fun tearDown() {
        middleware.reset()
    }

    @Test
    fun `GIVEN existing Session and Tab WHEN default search invoked THEN expected actions are dispatched`() {
        val id = "mozilla"
        store.dispatch(
            TabListAction.AddTabAction(
                tab = createTab(url = "https://www.mozilla.org", id = id),
                select = true,
            ),
        ).joinBlocking()

        useCases.defaultSearch(
            searchTerms = searchTerms,
            searchEngine = searchEngine,
        )
        store.waitUntilIdle()

        val isSearchAction = middleware.findFirstAction(ContentAction.UpdateIsSearchAction::class)
        assertEquals(id, isSearchAction.sessionId)
        assertEquals(true, isSearchAction.isSearch)
        assertEquals(searchEngineName, isSearchAction.searchEngineName)

        middleware.assertLastAction(EngineAction.LoadUrlAction::class) { action ->
            assertEquals(id, action.tabId)
            assertEquals(searchUrl, action.url)
        }
    }

    @Test
    fun `GIVEN existing Session, no existing Tab WHEN default search invoked THEN add tab is called`() {
        val newTabUseCase: TabsUseCases.AddNewTabUseCase = mock()
        whenever(tabsUseCases.addTab).thenReturn(newTabUseCase)
        val newTabId = "9876"
        whenever(
            newTabUseCase(
                url = searchUrl,
                isSearch = true,
                searchEngineName = searchEngineName,
            ),
        ).thenReturn(newTabId)

        useCases.defaultSearch(
            searchTerms = searchTerms,
            sessionId = "mozilla",
            searchEngine = searchEngine,
        )
        store.waitUntilIdle()

        verify(newTabUseCase).invoke(
            url = searchUrl,
            isSearch = true,
            searchEngineName = searchEngineName,
        )

        middleware.assertLastAction(ContentAction.UpdateSearchTermsAction::class) { action ->
            assertEquals(newTabId, action.sessionId)
            assertEquals(searchTerms, action.searchTerms)
        }
    }

    @Test
    fun defaultSearchOnNewSession() {
        val searchTerms = "mozilla android"

        val newTabUseCase: TabsUseCases.AddNewTabUseCase = mock()
        whenever(tabsUseCases.addTab).thenReturn(newTabUseCase)
        whenever(newTabUseCase(searchUrl, isSearch = true)).thenReturn("2342")

        useCases.newTabSearch(searchTerms, SessionState.Source.Internal.NewTab)
        store.waitUntilIdle()

        verify(newTabUseCase).invoke(
            searchUrl,
            parentId = null,
            selectTab = true,
            source = SessionState.Source.Internal.NewTab,
            isSearch = true,
        )

        val searchTermsAction = middleware.findFirstAction(ContentAction.UpdateSearchTermsAction::class)
        assertEquals("2342", searchTermsAction.sessionId)
        assertEquals(searchTerms, searchTermsAction.searchTerms)
    }

    @Test
    fun `GIVEN additional headers and a load url flag WHEN NewTabSearchUseCase creates a new tab THEN addTab is called`() {
        val source = SessionState.Source.Internal.UserEntered
        val flags = LoadUrlFlags.select(LoadUrlFlags.ALLOW_JAVASCRIPT_URL)
        val additionalHeaders = mapOf("X-Extra-Header" to "true")
        val sessionId = "2342"

        val newTabUseCase: TabsUseCases.AddNewTabUseCase = mock()
        whenever(tabsUseCases.addTab).thenReturn(newTabUseCase)
        whenever(
            newTabUseCase(
                url = searchUrl,
                isSearch = true,
                flags = flags,
                source = source,
                additionalHeaders = additionalHeaders,
            ),
        ).thenReturn(sessionId)

        useCases.newTabSearch(
            searchTerms = searchTerms,
            source = source,
            flags = flags,
            additionalHeaders = additionalHeaders,
        )
        store.waitUntilIdle()

        verify(newTabUseCase).invoke(
            url = searchUrl,
            flags = flags,
            source = source,
            isSearch = true,
            additionalHeaders = additionalHeaders,
        )

        val searchTermsAction =
            middleware.findFirstAction(ContentAction.UpdateSearchTermsAction::class)
        assertEquals(sessionId, searchTermsAction.sessionId)
        assertEquals(searchTerms, searchTermsAction.searchTerms)
    }

    @Test
    fun `DefaultSearchUseCase creates new tab if no session is selected`() {
        val newTabUseCase: TabsUseCases.AddNewTabUseCase = mock()
        whenever(tabsUseCases.addTab).thenReturn(newTabUseCase)
        whenever(newTabUseCase(searchUrl, isSearch = true)).thenReturn("2342")

        useCases.defaultSearch(searchTerms)
        store.waitUntilIdle()

        verify(newTabUseCase).invoke(
            searchUrl,
            parentId = null,
            selectTab = true,
            source = SessionState.Source.Internal.NewTab,
            isSearch = true,
        )

        val searchTermsAction = middleware.findFirstAction(ContentAction.UpdateSearchTermsAction::class)
        assertEquals("2342", searchTermsAction.sessionId)
        assertEquals(searchTerms, searchTermsAction.searchTerms)
    }

    @Test
    fun `GIVEN additional headers and a load url flag WHEN DefaultSearchUseCase creates new tab THEN addTab is called`() {
        val flags = LoadUrlFlags.select(LoadUrlFlags.ALLOW_JAVASCRIPT_URL)
        val additionalHeaders = mapOf("X-Extra-Header" to "true")
        val sessionId = "2342"

        val newTabUseCase: TabsUseCases.AddNewTabUseCase = mock()
        whenever(tabsUseCases.addTab).thenReturn(newTabUseCase)
        whenever(
            newTabUseCase(
                url = searchUrl,
                flags = flags,
                isSearch = true,
                searchEngineName = searchEngineName,
                additionalHeaders = additionalHeaders,
            ),
        ).thenReturn(sessionId)

        useCases.defaultSearch(
            searchTerms = searchTerms,
            searchEngine = searchEngine,
            flags = flags,
            additionalHeaders = additionalHeaders,
        )
        store.waitUntilIdle()

        verify(newTabUseCase).invoke(
            url = searchUrl,
            flags = flags,
            isSearch = true,
            searchEngineName = searchEngineName,
            additionalHeaders = additionalHeaders,
        )

        val searchTermsAction = middleware.findFirstAction(ContentAction.UpdateSearchTermsAction::class)
        assertEquals(sessionId, searchTermsAction.sessionId)
        assertEquals(searchTerms, searchTermsAction.searchTerms)
    }

    @Test
    fun newPrivateTabSearch() {
        val newTabUseCase: TabsUseCases.AddNewTabUseCase = mock()
        whenever(tabsUseCases.addTab).thenReturn(newTabUseCase)
        whenever(
            newTabUseCase(
                searchUrl,
                source = SessionState.Source.Internal.None,
                private = true,
                isSearch = true,
            ),
        ).thenReturn("1177")

        useCases.newPrivateTabSearch.invoke(searchTerms)
        store.waitUntilIdle()

        verify(newTabUseCase).invoke(
            searchUrl,
            parentId = null,
            selectTab = true,
            private = true,
            source = SessionState.Source.Internal.None,
            isSearch = true,
        )

        val searchTermsAction = middleware.findFirstAction(ContentAction.UpdateSearchTermsAction::class)
        assertEquals("1177", searchTermsAction.sessionId)
        assertEquals(searchTerms, searchTermsAction.searchTerms)
    }

    @Test
    fun newPrivateTabSearchWithParentSession() {
        val newTabUseCase: TabsUseCases.AddNewTabUseCase = mock()
        whenever(tabsUseCases.addTab).thenReturn(newTabUseCase)
        whenever(
            newTabUseCase(
                searchUrl,
                source = SessionState.Source.Internal.None,
                parentId = "test-parent",
                private = true,
                isSearch = true,
            ),
        ).thenReturn("1177")

        useCases.newPrivateTabSearch.invoke(searchTerms, parentSessionId = "test-parent")

        store.waitUntilIdle()

        verify(newTabUseCase).invoke(
            searchUrl,
            parentId = "test-parent",
            selectTab = true,
            private = true,
            source = SessionState.Source.Internal.None,
            isSearch = true,
        )

        val searchTermsAction = middleware.findFirstAction(ContentAction.UpdateSearchTermsAction::class)
        assertEquals("1177", searchTermsAction.sessionId)
        assertEquals(searchTerms, searchTermsAction.searchTerms)
    }

    @Test
    fun `Selecting search engine`() {
        val store = BrowserStore(getBrowserState())

        val useCases = SearchUseCases(store, mock(), mock())

        useCases.selectSearchEngine.invoke(
            store.findSearchEngineById("engine-d"),
        )

        store.waitUntilIdle()

        assertEquals("engine-d", store.state.search.userSelectedSearchEngineId)
        assertNull(store.state.search.userSelectedSearchEngineName)

        useCases.selectSearchEngine.invoke(
            store.findSearchEngineById("engine-b"),
        )

        store.waitUntilIdle()

        assertEquals("engine-b", store.state.search.userSelectedSearchEngineId)
        assertEquals("Engine B", store.state.search.userSelectedSearchEngineName)

        useCases.selectSearchEngine.invoke(
            store.findSearchEngineById("engine-f"),
        )

        store.waitUntilIdle()

        assertEquals("engine-f", store.state.search.userSelectedSearchEngineId)
        assertNull(store.state.search.userSelectedSearchEngineName)
    }

    @Test
    fun `addSearchEngine - add bundled engine`() {
        val store = BrowserStore(getBrowserState())

        val useCases = SearchUseCases(store, mock(), mock())

        assertEquals(7, store.state.search.searchEngines.size)
        assertEquals(3, store.state.search.availableSearchEngines.size)

        useCases.addSearchEngine.invoke(
            store.findSearchEngineById("engine-i"),
        )

        store.waitUntilIdle()

        assertEquals(8, store.state.search.searchEngines.size)
        assertEquals(2, store.state.search.availableSearchEngines.size)

        assertEquals(4, store.state.search.regionSearchEngines.size)
        assertEquals(0, store.state.search.hiddenSearchEngines.size)

        assertEquals("engine-i", store.state.search.regionSearchEngines[3].id)
        assertEquals("Engine I", store.state.search.regionSearchEngines[3].name)
    }

    @Test
    fun `addSearchEngine - add additional bundled engine`() {
        val store = BrowserStore(getBrowserState())

        val useCases = SearchUseCases(store, mock(), mock())

        assertEquals(7, store.state.search.searchEngines.size)
        assertEquals(3, store.state.search.availableSearchEngines.size)

        useCases.addSearchEngine.invoke(
            store.findSearchEngineById("engine-h"),
        )

        store.waitUntilIdle()

        assertEquals(8, store.state.search.searchEngines.size)
        assertEquals(2, store.state.search.availableSearchEngines.size)

        assertEquals(1, store.state.search.additionalAvailableSearchEngines.size)
        assertEquals(2, store.state.search.additionalSearchEngines.size)

        assertEquals("engine-h", store.state.search.additionalSearchEngines[1].id)
        assertEquals("Engine H", store.state.search.additionalSearchEngines[1].name)
    }

    @Test
    fun `addSearchEngine - add custom engine`() {
        val store = BrowserStore(getBrowserState())

        val useCases = SearchUseCases(store, mock(), mock())

        assertEquals(7, store.state.search.searchEngines.size)
        assertEquals(3, store.state.search.availableSearchEngines.size)

        useCases.addSearchEngine.invoke(
            createSearchEngine(
                name = "Engine X",
                url = "https://www.example.org/?q={searchTerms}",
                icon = mock(),
            ),
        )

        store.waitUntilIdle()

        assertEquals(8, store.state.search.searchEngines.size)
        assertEquals(3, store.state.search.availableSearchEngines.size)

        assertEquals(3, store.state.search.customSearchEngines.size)
        assertEquals("Engine X", store.state.search.customSearchEngines[2].name)
        assertEquals(
            "https://www.example.org/?q={searchTerms}",
            store.state.search.customSearchEngines[2].resultUrls[0],
        )
    }

    @Test
    fun `removeSearchEngine - remove bundled engine`() {
        val store = BrowserStore(getBrowserState())

        val useCases = SearchUseCases(store, mock(), mock())

        assertEquals(7, store.state.search.searchEngines.size)
        assertEquals(3, store.state.search.availableSearchEngines.size)

        useCases.removeSearchEngine.invoke(
            store.findSearchEngineById("engine-b"),
        )

        store.waitUntilIdle()

        assertEquals(6, store.state.search.searchEngines.size)
        assertEquals(4, store.state.search.availableSearchEngines.size)

        assertEquals(2, store.state.search.regionSearchEngines.size)
        assertEquals(2, store.state.search.hiddenSearchEngines.size)

        assertEquals("engine-b", store.state.search.hiddenSearchEngines[1].id)
        assertEquals("Engine B", store.state.search.hiddenSearchEngines[1].name)
    }

    @Test
    fun `removeSearchEngine - remove additional bundled engine`() {
        val store = BrowserStore(getBrowserState())

        val useCases = SearchUseCases(store, mock(), mock())

        assertEquals(7, store.state.search.searchEngines.size)
        assertEquals(3, store.state.search.availableSearchEngines.size)

        useCases.removeSearchEngine.invoke(
            store.findSearchEngineById("engine-f"),
        )

        store.waitUntilIdle()

        assertEquals(6, store.state.search.searchEngines.size)
        assertEquals(4, store.state.search.availableSearchEngines.size)

        assertEquals(0, store.state.search.additionalSearchEngines.size)
        assertEquals(3, store.state.search.additionalAvailableSearchEngines.size)

        assertEquals("engine-f", store.state.search.additionalAvailableSearchEngines[2].id)
        assertEquals("Engine F", store.state.search.additionalAvailableSearchEngines[2].name)
    }

    @Test
    fun `removeSearchEngine - remove custom engine`() {
        val store = BrowserStore(getBrowserState())

        val useCases = SearchUseCases(store, mock(), mock())

        assertEquals(7, store.state.search.searchEngines.size)
        assertEquals(3, store.state.search.availableSearchEngines.size)

        useCases.removeSearchEngine.invoke(
            store.findSearchEngineById("engine-d"),
        )

        store.waitUntilIdle()

        assertEquals(6, store.state.search.searchEngines.size)
        assertEquals(3, store.state.search.availableSearchEngines.size)

        assertEquals(1, store.state.search.customSearchEngines.size)
    }

    @Test
    fun `GIVEN disable search engine use case is invoked WHEN engine gets unselected THEN ID is stored in search state`() {
        val store = BrowserStore(getBrowserState())
        val useCases = SearchUseCases(store, mock(), mock())

        assertEquals(0, store.state.search.disabledSearchEngineIds.size)

        useCases.updateDisabledSearchEngineIds.invoke(
            searchEngineId = "engine-d",
            isEnabled = false,
        )
        store.waitUntilIdle()

        assertEquals(1, store.state.search.disabledSearchEngineIds.size)
    }

    @Test
    fun `GIVEN disable search engine use case is invoked WHEN engine gets selected THEN ID is removed from search state`() {
        val store = BrowserStore(getBrowserState(disabledSearchEngineIds = listOf("engine-d")))
        val useCases = SearchUseCases(store, mock(), mock())

        assertEquals(1, store.state.search.disabledSearchEngineIds.size)

        useCases.updateDisabledSearchEngineIds.invoke(
            searchEngineId = "engine-d",
            isEnabled = true,
        )
        store.waitUntilIdle()

        assertEquals(0, store.state.search.disabledSearchEngineIds.size)
    }

    @Test
    fun `WHEN restore search engines use case is invoked GIVEN there are hidden engines THEN hidden engines are added back to the bundled engine list`() {
        val regionSearchEngines = listOf(
            SearchEngine("bundled-engine-a", "Regional Engine A", mock(), type = SearchEngine.Type.BUNDLED),
            SearchEngine("bundled-engine-b", "Regional Engine B", mock(), type = SearchEngine.Type.BUNDLED),
        )

        val hiddenEngine = SearchEngine(
            "bundled-engine-c",
            "Regional Engine C",
            mock(),
            type = SearchEngine.Type.BUNDLED,
        )

        val store = BrowserStore(getBrowserState(hiddenSearchEngine = listOf(hiddenEngine), regionSearchEngines = regionSearchEngines))
        val useCases = SearchUseCases(store, mock(), mock())

        assertEquals(2, store.state.search.regionSearchEngines.size)
        assertEquals(1, store.state.search.hiddenSearchEngines.size)

        assertEquals("bundled-engine-a", store.state.search.regionSearchEngines[0].id)
        assertEquals("bundled-engine-b", store.state.search.regionSearchEngines[1].id)
        assertEquals("bundled-engine-c", store.state.search.hiddenSearchEngines[0].id)

        useCases.restoreHiddenSearchEngines.invoke()
        store.waitUntilIdle()

        assertEquals(3, store.state.search.regionSearchEngines.size)
        assertEquals(0, store.state.search.hiddenSearchEngines.size)

        assertEquals("bundled-engine-a", store.state.search.regionSearchEngines[0].id)
        assertEquals("bundled-engine-b", store.state.search.regionSearchEngines[1].id)
        assertEquals("bundled-engine-c", store.state.search.regionSearchEngines[2].id)
    }

    @Test
    fun `WHEN restore search engines use case is invoked GIVEN there are no hidden engines THEN do nothing`() {
        val regionSearchEngines = listOf(
            SearchEngine("bundled-engine-a", "Regional Engine A", mock(), type = SearchEngine.Type.BUNDLED),
            SearchEngine("bundled-engine-b", "Regional Engine B", mock(), type = SearchEngine.Type.BUNDLED),
            SearchEngine("bundled-engine-c", "Regional Engine C", mock(), type = SearchEngine.Type.BUNDLED),
        )
        val store = BrowserStore(getBrowserState(hiddenSearchEngine = emptyList(), regionSearchEngines = regionSearchEngines))
        val useCases = SearchUseCases(store, mock(), mock())

        assertEquals(0, store.state.search.hiddenSearchEngines.size)
        assertEquals(3, store.state.search.regionSearchEngines.size)

        assertEquals("bundled-engine-a", store.state.search.regionSearchEngines[0].id)
        assertEquals("bundled-engine-b", store.state.search.regionSearchEngines[1].id)
        assertEquals("bundled-engine-c", store.state.search.regionSearchEngines[2].id)

        useCases.restoreHiddenSearchEngines.invoke()
        store.waitUntilIdle()

        assertEquals(0, store.state.search.hiddenSearchEngines.size)
        assertEquals(3, store.state.search.regionSearchEngines.size)

        assertEquals("bundled-engine-a", store.state.search.regionSearchEngines[0].id)
        assertEquals("bundled-engine-b", store.state.search.regionSearchEngines[1].id)
        assertEquals("bundled-engine-c", store.state.search.regionSearchEngines[2].id)
    }
}

private fun getBrowserState(
    disabledSearchEngineIds: List<String> = emptyList(),
    regionSearchEngines: List<SearchEngine> = listOf(
        SearchEngine("engine-a", "Engine A", mock(), type = SearchEngine.Type.BUNDLED),
        SearchEngine("engine-b", "Engine B", mock(), type = SearchEngine.Type.BUNDLED),
        SearchEngine("engine-c", "Engine C", mock(), type = SearchEngine.Type.BUNDLED),
    ),
    hiddenSearchEngine: List<SearchEngine> = listOf(
        SearchEngine(
            "engine-i",
            "Engine I",
            mock(),
            type = SearchEngine.Type.BUNDLED,
        ),
    ),
) = BrowserState(
    search = SearchState(
        region = RegionState("US", "US"),
        regionSearchEngines = regionSearchEngines,
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
        hiddenSearchEngines = hiddenSearchEngine,
        disabledSearchEngineIds = disabledSearchEngineIds,
        regionDefaultSearchEngineId = "engine-b",
        userSelectedSearchEngineId = null,
        userSelectedSearchEngineName = null,
    ),
)

private fun BrowserStore.findSearchEngineById(id: String): SearchEngine {
    val searchEngine = (state.search.searchEngines + state.search.availableSearchEngines).find {
        it.id == id
    }
    return requireNotNull(searchEngine)
}
