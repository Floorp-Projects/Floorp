/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search

import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SearchUseCasesTest {

    private lateinit var searchEngine: SearchEngine
    private lateinit var searchEngineManager: SearchEngineManager
    private lateinit var sessionManager: SessionManager
    private lateinit var useCases: SearchUseCases

    @Before
    fun setup() {
        searchEngine = mock(SearchEngine::class.java)
        searchEngineManager = mock(SearchEngineManager::class.java)
        sessionManager = mock(SessionManager::class.java)
        useCases = SearchUseCases(testContext, searchEngineManager, sessionManager)
    }

    @Test
    fun defaultSearch() {
        val searchTerms = "mozilla android"
        val searchUrl = "http://search-url.com?$searchTerms"

        val session = Session("mozilla.org")
        val engineSession = mock(EngineSession::class.java)

        `when`(searchEngine.buildSearchUrl(searchTerms)).thenReturn(searchUrl)
        `when`(searchEngineManager.getDefaultSearchEngine(testContext)).thenReturn(searchEngine)
        `when`(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        useCases.defaultSearch.invoke(searchTerms, session)

        assertEquals(searchTerms, session.searchTerms)
        verify(engineSession).loadUrl(searchUrl)
    }

    @Test
    fun defaultSearchOnNewSession() {
        val searchTerms = "mozilla android"
        val searchUrl = "http://search-url.com?$searchTerms"

        val engineSession = mock(EngineSession::class.java)
        `when`(searchEngine.buildSearchUrl(searchTerms)).thenReturn(searchUrl)
        `when`(searchEngineManager.getDefaultSearchEngine(testContext)).thenReturn(searchEngine)
        `when`(sessionManager.getOrCreateEngineSession(any())).thenReturn(engineSession)

        useCases.newTabSearch.invoke(searchTerms, Session.Source.NEW_TAB)
        verify(engineSession).loadUrl(searchUrl)
    }

    @Test
    fun `DefaultSearchUseCase invokes onNoSession if no session is selected`() {
        var createdSession: Session? = null

        `when`(searchEngine.buildSearchUrl("test")).thenReturn("https://search.example.com")
        `when`(searchEngineManager.getDefaultSearchEngine(testContext)).thenReturn(searchEngine)
        `when`(sessionManager.getOrCreateEngineSession(any())).thenReturn(mock())

        var sessionCreatedForUrl: String? = null

        val searchUseCases = SearchUseCases(testContext, searchEngineManager, sessionManager) { url ->
            sessionCreatedForUrl = url
            Session(url).also { createdSession = it }
        }

        searchUseCases.defaultSearch.invoke("test")

        assertEquals("https://search.example.com", sessionCreatedForUrl)
        assertNotNull(createdSession)
        verify(sessionManager).getOrCreateEngineSession(createdSession!!)
    }

    @Test
    fun newPrivateTabSearch() {
        val searchTerms = "mozilla android"
        val searchUrl = "http://search-url.com?$searchTerms"

        val engineSession = mock(EngineSession::class.java)
        `when`(searchEngine.buildSearchUrl(searchTerms)).thenReturn(searchUrl)
        `when`(searchEngineManager.getDefaultSearchEngine(testContext)).thenReturn(searchEngine)
        `when`(sessionManager.getOrCreateEngineSession(any())).thenReturn(engineSession)

        useCases.newPrivateTabSearch.invoke(searchTerms)

        val captor = argumentCaptor<Session>()
        verify(sessionManager).add(captor.capture(), eq(true), eq(null), eq(null))
        assertTrue(captor.value.private)
        verify(engineSession).loadUrl(searchUrl)
    }
}
