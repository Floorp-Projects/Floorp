package mozilla.components.feature.search

import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.search.SearchRequest
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

private const val SELECTED_TAB_ID = "1"

class SearchFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var performSearch: (SearchRequest, String) -> Unit
    private lateinit var store: BrowserStore
    private lateinit var searchFeature: SearchFeature

    @Before
    fun before() {
        store = BrowserStore(
            mockBrowserState(),
        )
        performSearch = mock()
        searchFeature = SearchFeature(store, null, performSearch).apply {
            start()
        }
    }

    private fun mockBrowserState(): BrowserState {
        return BrowserState(
            tabs = listOf(
                createTab("https://www.duckduckgo.com", id = "0"),
                createTab("https://www.mozilla.org", id = SELECTED_TAB_ID),
                createTab("https://www.wikipedia.org", id = "2"),
            ),
            selectedTabId = SELECTED_TAB_ID,
        )
    }

    @After
    fun after() {
        searchFeature.stop()
    }

    @Test
    fun `GIVEN a tab is selected WHEN a search request is sent THEN a search should be performed`() {
        verify(performSearch, times(0)).invoke(any(), eq(SELECTED_TAB_ID))

        val normalSearchRequest = SearchRequest(isPrivate = false, query = "query")
        store.dispatch(ContentAction.UpdateSearchRequestAction(SELECTED_TAB_ID, normalSearchRequest)).joinBlocking()

        verify(performSearch, times(1)).invoke(any(), eq(SELECTED_TAB_ID))
        verify(performSearch, times(1)).invoke(normalSearchRequest, SELECTED_TAB_ID)

        val privateSearchRequest = SearchRequest(isPrivate = true, query = "query")
        store.dispatch(ContentAction.UpdateSearchRequestAction(SELECTED_TAB_ID, privateSearchRequest)).joinBlocking()

        verify(performSearch, times(2)).invoke(any(), eq(SELECTED_TAB_ID))
        verify(performSearch, times(1)).invoke(privateSearchRequest, SELECTED_TAB_ID)
    }

    @Test
    fun `GIVEN no tab is selected WHEN a search request is sent THEN no search should be performed`() {
        store.dispatch(TabListAction.RemoveTabAction(tabId = SELECTED_TAB_ID, selectParentIfExists = false))

        verify(performSearch, times(0)).invoke(any(), eq(SELECTED_TAB_ID))

        val normalSearchRequest = SearchRequest(isPrivate = false, query = "query")
        store.dispatch(ContentAction.UpdateSearchRequestAction(SELECTED_TAB_ID, normalSearchRequest)).joinBlocking()

        verify(performSearch, times(0)).invoke(any(), eq(SELECTED_TAB_ID))
        verify(performSearch, times(0)).invoke(normalSearchRequest, SELECTED_TAB_ID)

        val privateSearchRequest = SearchRequest(isPrivate = true, query = "query")
        store.dispatch(ContentAction.UpdateSearchRequestAction(SELECTED_TAB_ID, privateSearchRequest)).joinBlocking()

        verify(performSearch, times(0)).invoke(any(), eq(SELECTED_TAB_ID))
        verify(performSearch, times(0)).invoke(privateSearchRequest, SELECTED_TAB_ID)
    }

    @Test
    fun `WHEN a search request has been handled THEN that request should have been consumed`() {
        val normalSearchRequest = SearchRequest(isPrivate = false, query = "query")
        store.dispatch(ContentAction.UpdateSearchRequestAction(SELECTED_TAB_ID, normalSearchRequest)).joinBlocking()
        store.waitUntilIdle()

        assertNull(store.state.selectedTab!!.content.searchRequest)

        val privateSearchRequest = SearchRequest(isPrivate = true, query = "query")
        store.dispatch(ContentAction.UpdateSearchRequestAction(SELECTED_TAB_ID, privateSearchRequest)).joinBlocking()
        store.waitUntilIdle()

        assertNull(store.state.selectedTab!!.content.searchRequest)
    }

    @Test
    fun `WHEN the same search is requested two times THEN both search requests are preformed and consumed`() {
        val searchRequest = SearchRequest(isPrivate = false, query = "query")
        verify(performSearch, times(0)).invoke(searchRequest, SELECTED_TAB_ID)

        store.dispatch(ContentAction.UpdateSearchRequestAction(SELECTED_TAB_ID, searchRequest)).joinBlocking()
        store.waitUntilIdle()

        verify(performSearch, times(1)).invoke(searchRequest, SELECTED_TAB_ID)
        assertNull(store.state.selectedTab!!.content.searchRequest)

        store.dispatch(ContentAction.UpdateSearchRequestAction(SELECTED_TAB_ID, searchRequest)).joinBlocking()
        store.waitUntilIdle()

        verify(performSearch, times(2)).invoke(searchRequest, SELECTED_TAB_ID)
        assertNull(store.state.selectedTab!!.content.searchRequest)
    }
}
