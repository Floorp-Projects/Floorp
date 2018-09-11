/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.content.Intent
import android.support.customtabs.CustomTabsIntent
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.Session.Source
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyZeroInteractions
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SessionIntentProcessorTest {
    private val sessionManager = mock(SessionManager::class.java)
    private val session = mock(Session::class.java)
    private val engineSession = mock(EngineSession::class.java)
    private val useCases = SessionUseCases(sessionManager)

    @Before
    fun setup() {
        `when`(sessionManager.selectedSession).thenReturn(session)
        `when`(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
    }

    @Test
    fun testProcessWithDefaultHandlers() {
        val engine = mock(Engine::class.java)
        val sessionManager = spy(SessionManager(engine))
        val useCases = SessionUseCases(sessionManager)
        val handler = SessionIntentProcessor(useCases, sessionManager)
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
    fun testProcessWithDefaultHandlersUsingSelectedSession() {
        val handler = SessionIntentProcessor(useCases, sessionManager, true, false)
        val intent = mock(Intent::class.java)
        `when`(intent.action).thenReturn(Intent.ACTION_VIEW)
        `when`(intent.dataString).thenReturn("http://mozilla.org")

        handler.process(intent)
        verify(engineSession).loadUrl("http://mozilla.org")
    }

    @Test
    fun testProcessWithDefaultHandlersHavingNoSelectedSession() {
        `when`(sessionManager.selectedSession).thenReturn(null)
        doReturn(engineSession).`when`(sessionManager).getOrCreateEngineSession(anySession())

        val handler = SessionIntentProcessor(useCases, sessionManager, true, false)
        val intent = mock(Intent::class.java)
        `when`(intent.action).thenReturn(Intent.ACTION_VIEW)
        `when`(intent.dataString).thenReturn("http://mozilla.org")

        handler.process(intent)
        verify(engineSession).loadUrl("http://mozilla.org")
    }

    @Test
    fun testProcessWithoutDefaultHandlers() {
        val handler = SessionIntentProcessor(useCases, sessionManager, useDefaultHandlers = false)
        val intent = mock(Intent::class.java)
        `when`(intent.action).thenReturn(Intent.ACTION_VIEW)
        `when`(intent.dataString).thenReturn("http://mozilla.org")

        handler.process(intent)
        verifyZeroInteractions(engineSession)
    }

    @Test
    fun testProcessWithCustomHandlers() {
        val handler = SessionIntentProcessor(useCases, sessionManager, useDefaultHandlers = false)
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
    fun testProcessCustomTabIntentWithDefaultHandlers() {
        val engine = mock(Engine::class.java)
        val sessionManager = spy(SessionManager(engine))
        doReturn(engineSession).`when`(sessionManager).getOrCreateEngineSession(anySession())
        val useCases = SessionUseCases(sessionManager)

        val handler = SessionIntentProcessor(useCases, sessionManager)

        val intent = mock(Intent::class.java)
        `when`(intent.action).thenReturn(Intent.ACTION_VIEW)
        `when`(intent.hasExtra(CustomTabsIntent.EXTRA_SESSION)).thenReturn(true)
        `when`(intent.dataString).thenReturn("http://mozilla.org")

        handler.process(intent)
        verify(sessionManager).add(anySession(), eq(false), eq(null))
        verify(engineSession).loadUrl("http://mozilla.org")

        val customTabSession = sessionManager.all[0]
        assertNotNull(customTabSession)
        assertEquals("http://mozilla.org", customTabSession.url)
        assertEquals(Source.CUSTOM_TAB, customTabSession.source)
        assertNotNull(customTabSession.customTabConfig)
    }

    @Test
    fun `load url when action send with a valid url`() {
        doReturn(engineSession).`when`(sessionManager).getOrCreateEngineSession(anySession())

        val handler = SessionIntentProcessor(useCases, sessionManager)

        val intent = mock(Intent::class.java)
        `when`(intent.action).thenReturn(Intent.ACTION_SEND)
        `when`(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("http://mozilla.org")

        handler.process(intent)

        verify(engineSession).loadUrl("http://mozilla.org")
    }

    @Test
    fun `perform search when action send with text`() {
        var searchHandlerCalled = false

        val textSearchHandler = object : TextSearchHandler {
            override fun invoke(text: String, session: Session) {
                searchHandlerCalled = true
            }
        }

        val handler = SessionIntentProcessor(useCases, sessionManager, textSearchHandler = textSearchHandler)

        val intent = mock(Intent::class.java)
        `when`(intent.action).thenReturn(Intent.ACTION_SEND)
        `when`(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("mozilla android")

        handler.process(intent)

        assertTrue(searchHandlerCalled)
    }

    @Test
    fun `perform search when action send with empty text`() {
        val handler = SessionIntentProcessor(useCases, sessionManager)

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