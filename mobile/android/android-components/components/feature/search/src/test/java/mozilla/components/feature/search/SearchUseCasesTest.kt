/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search

import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.session.SessionProvider
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class SearchUseCasesTest {

    private val searchEngine = mock(SearchEngine::class.java)
    private val searchEngineManager = mock(SearchEngineManager::class.java)
    private val sessionProvider = mock(SessionProvider::class.java)
    private val useCases = SearchUseCases(RuntimeEnvironment.application, searchEngineManager, sessionProvider)

    @Test
    fun testDefaultSearch() {
        val searchTerms = "mozilla android"
        val searchUrl = "http://search-url.com?$searchTerms"

        val session = Session("mozilla.org")
        val engineSession = mock(EngineSession::class.java)

        `when`(searchEngine.buildSearchUrl(searchTerms)).thenReturn(searchUrl)
        `when`(searchEngineManager.getDefaultSearchEngine(RuntimeEnvironment.application)).thenReturn(searchEngine)
        `when`(sessionProvider.getEngineSession(session)).thenReturn(engineSession)

        useCases.defaultSearch.invoke(searchTerms, session)

        assertEquals(searchTerms, session.searchTerms)
        verify(engineSession).loadUrl(searchUrl)
    }
}