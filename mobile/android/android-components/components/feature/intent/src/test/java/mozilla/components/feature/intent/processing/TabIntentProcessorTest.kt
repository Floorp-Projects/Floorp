/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.intent.processing

import android.content.Intent
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.Session.Source
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class TabIntentProcessorTest {

    private val sessionManager = mock<SessionManager>()
    private val session = mock<Session>()
    private val engineSession = mock<EngineSession>()
    private val sessionUseCases = SessionUseCases(sessionManager)
    private val searchEngineManager = mock<SearchEngineManager>()
    private val searchUseCases = SearchUseCases(testContext, searchEngineManager, sessionManager)

    @Before
    fun setup() {
        whenever(sessionManager.selectedSession).thenReturn(session)
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
    }

    @Test
    fun processViewIntent() = runBlockingTest {
        val engine = mock<Engine>()
        val sessionManager = spy(SessionManager(engine))
        val useCases = SessionUseCases(sessionManager)
        val handler =
            TabIntentProcessor(sessionManager, useCases.loadUrl, searchUseCases.newTabSearch)
        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_VIEW)

        val engineSession = mock<EngineSession>()
        doReturn(engineSession).`when`(sessionManager).getOrCreateEngineSession(anySession())

        whenever(intent.dataString).thenReturn("")
        handler.process(intent)
        verify(engineSession, never()).loadUrl("")

        whenever(intent.dataString).thenReturn("http://mozilla.org")
        handler.process(intent)
        verify(engineSession).loadUrl("http://mozilla.org", LoadUrlFlags.external())

        val session = sessionManager.all[0]
        assertNotNull(session)
        assertEquals("http://mozilla.org", session.url)
        assertEquals(Source.ACTION_VIEW, session.source)
    }

    @Test
    fun processViewIntentUsingSelectedSession() = runBlockingTest {
        val handler = TabIntentProcessor(
            sessionManager,
            sessionUseCases.loadUrl,
            searchUseCases.newTabSearch,
            openNewTab = false
        )
        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_VIEW)
        whenever(intent.dataString).thenReturn("http://mozilla.org")

        handler.process(intent)
        verify(engineSession).loadUrl("http://mozilla.org", LoadUrlFlags.external())
    }

    @Test
    fun processViewIntentHavingNoSelectedSession() = runBlockingTest {
        whenever(sessionManager.selectedSession).thenReturn(null)
        doReturn(engineSession).`when`(sessionManager).getOrCreateEngineSession(anySession())

        val handler = TabIntentProcessor(
            sessionManager,
            sessionUseCases.loadUrl,
            searchUseCases.newTabSearch,
            openNewTab = false
        )
        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_VIEW)
        whenever(intent.dataString).thenReturn("http://mozilla.org")

        handler.process(intent)
        verify(engineSession).loadUrl("http://mozilla.org", LoadUrlFlags.external())
    }

    @Test
    fun `load URL on ACTION_SEND if text contains URL`() = runBlockingTest {
        doReturn(engineSession).`when`(sessionManager).getOrCreateEngineSession(anySession())

        val handler = TabIntentProcessor(sessionManager, sessionUseCases.loadUrl, searchUseCases.newTabSearch)

        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_SEND)

        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("http://mozilla.org")
        handler.process(intent)
        verify(engineSession).loadUrl("http://mozilla.org", LoadUrlFlags.external())

        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("see http://getpocket.com")
        handler.process(intent)
        verify(engineSession).loadUrl("http://getpocket.com", LoadUrlFlags.external())

        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("see http://mozilla.com and http://getpocket.com")
        handler.process(intent)
        verify(engineSession).loadUrl("http://mozilla.com", LoadUrlFlags.external())

        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("checkout the Tweet: http://tweets.mozilla.com")
        handler.process(intent)
        verify(engineSession).loadUrl("http://tweets.mozilla.com", LoadUrlFlags.external())

        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("checkout the Tweet: HTTP://tweets.mozilla.com")
        handler.process(intent)
        verify(engineSession).loadUrl("http://tweets.mozilla.com", LoadUrlFlags.external())
    }

    @Test
    fun `perform search on ACTION_SEND if text (no URL) provided`() = runBlockingTest {
        val engine = mock<Engine>()
        val sessionManager = spy(SessionManager(engine))
        doReturn(engineSession).`when`(sessionManager).getOrCreateEngineSession(anySession())

        val searchUseCases = SearchUseCases(testContext, searchEngineManager, sessionManager)
        val sessionUseCases = SessionUseCases(sessionManager)

        val searchTerms = "mozilla android"
        val searchUrl = "http://search-url.com?$searchTerms"

        val handler = TabIntentProcessor(sessionManager, sessionUseCases.loadUrl, searchUseCases.newTabSearch)

        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_SEND)
        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn(searchTerms)

        val searchEngine = mock<SearchEngine>()
        whenever(searchEngine.buildSearchUrl(searchTerms)).thenReturn(searchUrl)
        whenever(searchEngineManager.getDefaultSearchEngine(testContext)).thenReturn(searchEngine)

        handler.process(intent)
        verify(engineSession).loadUrl(searchUrl)
        assertEquals(searchUrl, sessionManager.selectedSession?.url)
        assertEquals(searchTerms, sessionManager.selectedSession?.searchTerms)
    }

    @Test
    fun `processor handles ACTION_SEND with empty text`() = runBlockingTest {
        val handler = TabIntentProcessor(sessionManager, sessionUseCases.loadUrl, searchUseCases.newTabSearch)

        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_SEND)
        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn(" ")

        val processed = handler.process(intent)
        assertFalse(processed)
    }

    @Suppress("UNCHECKED_CAST")
    private fun <T> anySession(): T {
        any<T>()
        return null as T
    }
}
