/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.intent

import android.content.Intent
import android.support.customtabs.CustomTabsIntent
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.Session.Source
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.test.any
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyZeroInteractions
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class IntentProcessorTest {
    private val sessionManager = mock(SessionManager::class.java)
    private val session = mock(Session::class.java)
    private val engineSession = mock(EngineSession::class.java)
    private val sessionUseCases = SessionUseCases(sessionManager)
    private val searchEngineManager = mock(SearchEngineManager::class.java)
    private val searchUseCases = SearchUseCases(RuntimeEnvironment.application, searchEngineManager, sessionManager)

    @Before
    fun setup() {
        `when`(sessionManager.selectedSession).thenReturn(session)
        `when`(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
    }

    @Test
    fun processWithDefaultHandlers() {
        val engine = mock(Engine::class.java)
        val sessionManager = spy(SessionManager(engine))
        val useCases = SessionUseCases(sessionManager)
        val handler =
            IntentProcessor(useCases, sessionManager, searchUseCases, RuntimeEnvironment.application)
        val intent = mock(Intent::class.java)
        `when`(intent.action).thenReturn(Intent.ACTION_VIEW)

        val engineSession = mock(EngineSession::class.java)
        doReturn(engineSession).`when`(sessionManager).getOrCreateEngineSession(anySession())

        `when`(intent.dataString).thenReturn("")
        handler.process(intent)
        verify(engineSession, never()).loadUrl("")

        `when`(intent.dataString).thenReturn("http://mozilla.org")
        handler.process(intent)
        verify(engineSession).loadUrl("http://mozilla.org")

        val session = sessionManager.all[0]
        assertNotNull(session)
        assertEquals("http://mozilla.org", session.url)
        assertEquals(Source.ACTION_VIEW, session.source)
    }

    @Test
    fun processWithDefaultHandlersUsingSelectedSession() {
        val handler = IntentProcessor(
            sessionUseCases,
            sessionManager,
            searchUseCases,
            RuntimeEnvironment.application,
            true,
            false
        )
        val intent = mock(Intent::class.java)
        `when`(intent.action).thenReturn(Intent.ACTION_VIEW)
        `when`(intent.dataString).thenReturn("http://mozilla.org")

        handler.process(intent)
        verify(engineSession).loadUrl("http://mozilla.org")
    }

    @Test
    fun processWithDefaultHandlersHavingNoSelectedSession() {
        `when`(sessionManager.selectedSession).thenReturn(null)
        doReturn(engineSession).`when`(sessionManager).getOrCreateEngineSession(anySession())

        val handler = IntentProcessor(
            sessionUseCases,
            sessionManager,
            searchUseCases,
            RuntimeEnvironment.application,
            true,
            false
        )
        val intent = mock(Intent::class.java)
        `when`(intent.action).thenReturn(Intent.ACTION_VIEW)
        `when`(intent.dataString).thenReturn("http://mozilla.org")

        handler.process(intent)
        verify(engineSession).loadUrl("http://mozilla.org")
    }

    @Test
    fun processWithoutDefaultHandlers() {
        val handler = IntentProcessor(
            sessionUseCases,
            sessionManager,
            searchUseCases,
            RuntimeEnvironment.application,
            useDefaultHandlers = false
        )
        val intent = mock(Intent::class.java)
        `when`(intent.action).thenReturn(Intent.ACTION_VIEW)
        `when`(intent.dataString).thenReturn("http://mozilla.org")

        handler.process(intent)
        verifyZeroInteractions(engineSession)
    }

    @Test
    fun processWithCustomHandlers() {
        val handler = IntentProcessor(
            sessionUseCases,
            sessionManager,
            searchUseCases,
            RuntimeEnvironment.application,
            useDefaultHandlers = false
        )
        val intent = mock(Intent::class.java)
        `when`(intent.action).thenReturn(Intent.ACTION_SEND)

        var handlerInvoked = false
        handler.registerHandler(Intent.ACTION_SEND) { _ ->
            handlerInvoked = true
            true
        }

        handler.process(intent)
        assertTrue(handlerInvoked)

        handlerInvoked = false
        handler.unregisterHandler(Intent.ACTION_SEND)

        handler.process(intent)
        assertFalse(handlerInvoked)
    }

    @Test
    fun processCustomTabIntentWithDefaultHandlers() {
        val engine = mock(Engine::class.java)
        val sessionManager = spy(SessionManager(engine))
        doReturn(engineSession).`when`(sessionManager).getOrCreateEngineSession(anySession())
        val useCases = SessionUseCases(sessionManager)

        val handler = IntentProcessor(useCases, sessionManager, searchUseCases, RuntimeEnvironment.application)

        val intent = mock(Intent::class.java)
        `when`(intent.action).thenReturn(Intent.ACTION_VIEW)
        `when`(intent.hasExtra(CustomTabsIntent.EXTRA_SESSION)).thenReturn(true)
        `when`(intent.dataString).thenReturn("http://mozilla.org")

        handler.process(intent)
        verify(sessionManager).add(anySession(), eq(false), eq(null), eq(null))
        verify(engineSession).loadUrl("http://mozilla.org")

        val customTabSession = sessionManager.all[0]
        assertNotNull(customTabSession)
        assertEquals("http://mozilla.org", customTabSession.url)
        assertEquals(Source.CUSTOM_TAB, customTabSession.source)
        assertNotNull(customTabSession.customTabConfig)
    }

    @Test
    fun `load URL on ACTION_SEND if text contains URL`() {
        doReturn(engineSession).`when`(sessionManager).getOrCreateEngineSession(anySession())

        val handler = IntentProcessor(sessionUseCases, sessionManager, searchUseCases, RuntimeEnvironment.application)

        val intent = mock(Intent::class.java)
        `when`(intent.action).thenReturn(Intent.ACTION_SEND)

        `when`(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("http://mozilla.org")
        handler.process(intent)
        verify(engineSession).loadUrl("http://mozilla.org")

        `when`(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("see http://getpocket.com")
        handler.process(intent)
        verify(engineSession).loadUrl("http://getpocket.com")

        `when`(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("see http://mozilla.com and http://getpocket.com")
        handler.process(intent)
        verify(engineSession).loadUrl("http://mozilla.com")

        `when`(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("checkout the Tweet: http://tweets.mozilla.com")
        handler.process(intent)
        verify(engineSession).loadUrl("http://tweets.mozilla.com")

        `when`(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("checkout the Tweet: HTTP://tweets.mozilla.com")
        handler.process(intent)
        verify(engineSession).loadUrl("http://tweets.mozilla.com")
    }

    @Test
    fun `perform search on ACTION_SEND if text (no URL) provided`() {
        val engine = mock(Engine::class.java)
        val sessionManager = spy(SessionManager(engine))
        doReturn(engineSession).`when`(sessionManager).getOrCreateEngineSession(anySession())

        val searchUseCases = SearchUseCases(RuntimeEnvironment.application, searchEngineManager, sessionManager)
        val sessionUseCases = SessionUseCases(sessionManager)

        val searchTerms = "mozilla android"
        val searchUrl = "http://search-url.com?$searchTerms"

        val handler = IntentProcessor(sessionUseCases, sessionManager, searchUseCases, RuntimeEnvironment.application)

        val intent = mock(Intent::class.java)
        `when`(intent.action).thenReturn(Intent.ACTION_SEND)
        `when`(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn(searchTerms)

        val searchEngine = mock(SearchEngine::class.java)
        `when`(searchEngine.buildSearchUrl(searchTerms)).thenReturn(searchUrl)
        `when`(searchEngineManager.getDefaultSearchEngine(RuntimeEnvironment.application)).thenReturn(searchEngine)

        handler.process(intent)
        verify(engineSession).loadUrl(searchUrl)
        assertEquals(searchUrl, sessionManager.selectedSession?.url)
        assertEquals(searchTerms, sessionManager.selectedSession?.searchTerms)
    }

    @Test
    fun `processor handles ACTION_SEND with empty text`() {
        val handler = IntentProcessor(sessionUseCases, sessionManager, searchUseCases, RuntimeEnvironment.application)

        val intent = mock(Intent::class.java)
        `when`(intent.action).thenReturn(Intent.ACTION_SEND)
        `when`(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn(" ")

        val processed = handler.process(intent)
        assertFalse(processed)
    }

    @Suppress("UNCHECKED_CAST")
    private fun <T> anySession(): T {
        any<T>()
        return null as T
    }
}