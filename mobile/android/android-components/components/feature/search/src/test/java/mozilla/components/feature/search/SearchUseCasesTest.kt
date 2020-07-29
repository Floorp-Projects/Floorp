/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class SearchUseCasesTest {

    private lateinit var searchEngine: SearchEngine
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
        useCases = SearchUseCases(testContext, store, searchEngineManager, sessionManager)
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

        val searchUseCases = SearchUseCases(testContext, store, searchEngineManager, sessionManager) { url ->
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
        useCases.newPrivateTabSearch.invoke(searchTerms, parentSession = parentSession)

        val captor = argumentCaptor<Session>()
        verify(sessionManager).add(captor.capture(), eq(true), eq(null), eq(null), eq(parentSession))
        assertTrue(captor.value.private)

        val actionCaptor = argumentCaptor<EngineAction.LoadUrlAction>()
        verify(store).dispatch(actionCaptor.capture())
        assertEquals(searchUrl, actionCaptor.value.url)
    }
}
