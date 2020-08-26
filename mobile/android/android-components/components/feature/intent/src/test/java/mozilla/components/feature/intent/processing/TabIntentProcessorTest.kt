/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.intent.processing

import android.app.SearchManager
import android.content.Intent
import android.nfc.NfcAdapter.ACTION_NDEF_DISCOVERED
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.state.SessionState.Source
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyList
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class TabIntentProcessorTest {

    private val store = mock<BrowserStore>()
    private val sessionManager = mock<SessionManager>()
    private val session = mock<Session>()
    private val sessionUseCases = SessionUseCases(store, sessionManager)
    private val searchEngineManager = mock<SearchEngineManager>()
    private val searchUseCases = SearchUseCases(testContext, store, searchEngineManager, sessionManager)

    @Before
    fun setup() {
        whenever(sessionManager.selectedSession).thenReturn(session)
    }

    @Test
    fun processViewIntent() {
        val engine = mock<Engine>()
        val sessionManager = spy(SessionManager(engine))
        val useCases = SessionUseCases(store, sessionManager)
        val handler =
            TabIntentProcessor(sessionManager, useCases.loadUrl, searchUseCases.newTabSearch)
        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_VIEW)

        val engineSession = mock<EngineSession>()
        whenever(intent.dataString).thenReturn("")
        handler.process(intent)
        verify(engineSession, never()).loadUrl("")

        whenever(intent.dataString).thenReturn("http://mozilla.org")
        handler.process(intent)

        val sessionCaptor = argumentCaptor<Session>()
        verify(sessionManager).add(sessionCaptor.capture(), eq(true), eq(null), eq(null), eq(null))
        verify(store).dispatch(EngineAction.LoadUrlAction(
            sessionCaptor.value.id, "http://mozilla.org", LoadUrlFlags.external())
        )

        // try to send a request to open a tab with the same url as before
        whenever(intent.dataString).thenReturn("http://mozilla.org")
        handler.process(intent)
        verify(sessionManager).select(any())
        verify(sessionManager, never()).add(anyList())

        assertEquals(sessionManager.sessions.size, 1)

        val session = sessionManager.all[0]
        assertNotNull(session)
        assertEquals("http://mozilla.org", session.url)
        assertEquals(Source.ACTION_VIEW, session.source)
    }

    @Test
    fun processViewIntentUsingSelectedSession() {
        val engine = mock<Engine>()
        val sessionManager = spy(SessionManager(engine))
        val session = Session("http://mozilla.org")
        val handler = TabIntentProcessor(
            sessionManager,
            sessionUseCases.loadUrl,
            searchUseCases.newTabSearch,
            openNewTab = false
        )

        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_VIEW)
        whenever(intent.dataString).thenReturn("http://mozilla.org")
        sessionManager.add(session)

        handler.process(intent)
        verify(sessionManager).select(session)
        verify(store, never()).dispatch(any())
    }

    @Test
    fun processViewIntentHavingNoSelectedSession() {
        whenever(sessionManager.selectedSession).thenReturn(null)

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
        val actionCaptor = argumentCaptor<EngineAction.LoadUrlAction>()
        verify(store).dispatch(actionCaptor.capture())
        assertEquals("http://mozilla.org", actionCaptor.value.url)
    }

    @Test
    fun processNfcIntent() {
        val engine = mock<Engine>()
        val sessionManager = spy(SessionManager(engine))
        val useCases = SessionUseCases(store, sessionManager)
        val handler =
            TabIntentProcessor(sessionManager, useCases.loadUrl, searchUseCases.newTabSearch)
        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(ACTION_NDEF_DISCOVERED)

        val engineSession = mock<EngineSession>()

        whenever(intent.dataString).thenReturn("")
        handler.process(intent)
        verify(engineSession, never()).loadUrl("")

        whenever(intent.dataString).thenReturn("http://mozilla.org")
        handler.process(intent)
        val sessionCaptor = argumentCaptor<Session>()
        verify(sessionManager).add(sessionCaptor.capture(), eq(true), eq(null), eq(null), eq(null))
        verify(store).dispatch(EngineAction.LoadUrlAction(
            sessionCaptor.value.id, "http://mozilla.org", LoadUrlFlags.external())
        )

        val session = sessionManager.all[0]
        assertNotNull(session)
        assertEquals("http://mozilla.org", session.url)
        assertEquals(Source.ACTION_VIEW, session.source)
    }

    @Test
    fun processNfcIntentUsingSelectedSession() {
        val engine = mock<Engine>()
        val sessionManager = spy(SessionManager(engine))
        val session = Session("http://mozilla.org")
        val handler = TabIntentProcessor(
            sessionManager,
            sessionUseCases.loadUrl,
            searchUseCases.newTabSearch,
            openNewTab = false
        )
        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(ACTION_NDEF_DISCOVERED)
        whenever(intent.dataString).thenReturn("http://mozilla.org")
        sessionManager.add(session)

        handler.process(intent)
        verify(sessionManager).select(session)
        verify(store, never()).dispatch(any())
    }

    @Test
    fun processNfcIntentHavingNoSelectedSession() {
        whenever(sessionManager.selectedSession).thenReturn(null)

        val handler = TabIntentProcessor(
            sessionManager,
            sessionUseCases.loadUrl,
            searchUseCases.newTabSearch,
            openNewTab = false
        )
        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(ACTION_NDEF_DISCOVERED)
        whenever(intent.dataString).thenReturn("http://mozilla.org")

        handler.process(intent)
        val actionCaptor = argumentCaptor<EngineAction.LoadUrlAction>()
        verify(store).dispatch(actionCaptor.capture())
        assertEquals("http://mozilla.org", actionCaptor.value.url)
    }

    @Test
    fun `load URL on ACTION_SEND if text contains URL`() {
        val handler = TabIntentProcessor(sessionManager, sessionUseCases.loadUrl, searchUseCases.newTabSearch)

        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_SEND)

        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("http://mozilla.org")
        handler.process(intent)
        val sessionCaptor = argumentCaptor<Session>()
        verify(sessionManager).add(sessionCaptor.capture(), eq(true), eq(null), eq(null), eq(null))
        verify(store).dispatch(EngineAction.LoadUrlAction(
            sessionCaptor.value.id, "http://mozilla.org", LoadUrlFlags.external())
        )

        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("see http://getpocket.com")
        handler.process(intent)
        verify(sessionManager, times(2)).add(sessionCaptor.capture(), eq(true), eq(null), eq(null), eq(null))
        verify(store).dispatch(EngineAction.LoadUrlAction(
            sessionCaptor.allValues.last().id, "http://getpocket.com", LoadUrlFlags.external())
        )

        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("see http://mozilla.com and http://getpocket.com")
        handler.process(intent)
        verify(sessionManager, times(3)).add(sessionCaptor.capture(), eq(true), eq(null), eq(null), eq(null))
        verify(store).dispatch(EngineAction.LoadUrlAction(
            sessionCaptor.allValues.last().id, "http://mozilla.com", LoadUrlFlags.external())
        )

        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("checkout the Tweet: http://tweets.mozilla.com")
        handler.process(intent)
        verify(sessionManager, times(4)).add(sessionCaptor.capture(), eq(true), eq(null), eq(null), eq(null))
        verify(store).dispatch(EngineAction.LoadUrlAction(
            sessionCaptor.allValues.last().id, "http://tweets.mozilla.com", LoadUrlFlags.external())
        )

        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("checkout the Tweet: HTTP://tweets.mozilla.org")
        handler.process(intent)
        verify(sessionManager, times(5)).add(sessionCaptor.capture(), eq(true), eq(null), eq(null), eq(null))
        verify(store).dispatch(EngineAction.LoadUrlAction(
            sessionCaptor.allValues.last().id, "HTTP://tweets.mozilla.org", LoadUrlFlags.external())
        )
    }

    @Test
    fun `perform search on ACTION_SEND if text (no URL) provided`() {
        val engine = mock<Engine>()
        val sessionManager = spy(SessionManager(engine))

        val searchUseCases = SearchUseCases(testContext, store, searchEngineManager, sessionManager)
        val sessionUseCases = SessionUseCases(store, sessionManager)

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
        val sessionCaptor = argumentCaptor<Session>()
        verify(sessionManager).add(sessionCaptor.capture(), eq(true), eq(null), eq(null), eq(null))
        verify(store).dispatch(EngineAction.LoadUrlAction(
            sessionCaptor.value.id, searchUrl, LoadUrlFlags.none())
        )
        assertEquals(searchUrl, sessionManager.selectedSession?.url)
        assertEquals(searchTerms, sessionManager.selectedSession?.searchTerms)
    }

    @Test
    fun `processor handles ACTION_SEND with empty text`() {
        val handler = TabIntentProcessor(sessionManager, sessionUseCases.loadUrl, searchUseCases.newTabSearch)

        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_SEND)
        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn(" ")

        val processed = handler.process(intent)
        assertFalse(processed)
    }

    @Test
    fun `processor handles ACTION_SEARCH with empty text`() {
        val handler = TabIntentProcessor(sessionManager, sessionUseCases.loadUrl, searchUseCases.newTabSearch)

        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_SEARCH)
        whenever(intent.getStringExtra(SearchManager.QUERY)).thenReturn(" ")

        val processed = handler.process(intent)
        assertFalse(processed)
    }

    @Test
    fun `load URL on ACTION_SEARCH if text is an URL`() {
        val handler = TabIntentProcessor(sessionManager, sessionUseCases.loadUrl, searchUseCases.newTabSearch)

        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_SEARCH)

        whenever(intent.getStringExtra(SearchManager.QUERY)).thenReturn("http://mozilla.org")
        handler.process(intent)
        val sessionCaptor = argumentCaptor<Session>()
        verify(sessionManager).add(sessionCaptor.capture(), eq(true), eq(null), eq(null), eq(null))
        verify(store).dispatch(EngineAction.LoadUrlAction(
            sessionCaptor.value.id, "http://mozilla.org", LoadUrlFlags.external())
        )
    }

    @Test
    fun `perform search on ACTION_SEARCH if text (no URL) provided`() {
        val engine = mock<Engine>()
        val sessionManager = spy(SessionManager(engine))

        val searchUseCases = SearchUseCases(testContext, store, searchEngineManager, sessionManager)
        val sessionUseCases = SessionUseCases(store, sessionManager)

        val searchTerms = "mozilla android"
        val searchUrl = "http://search-url.com?$searchTerms"

        val handler = TabIntentProcessor(sessionManager, sessionUseCases.loadUrl, searchUseCases.newTabSearch)

        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_SEARCH)
        whenever(intent.getStringExtra(SearchManager.QUERY)).thenReturn(searchTerms)

        val searchEngine = mock<SearchEngine>()
        whenever(searchEngine.buildSearchUrl(searchTerms)).thenReturn(searchUrl)
        whenever(searchEngineManager.getDefaultSearchEngine(testContext)).thenReturn(searchEngine)

        handler.process(intent)
        val sessionCaptor = argumentCaptor<Session>()
        verify(sessionManager).add(sessionCaptor.capture(), eq(true), eq(null), eq(null), eq(null))
        verify(store).dispatch(EngineAction.LoadUrlAction(
            sessionCaptor.value.id, searchUrl, LoadUrlFlags.none())
        )
        assertEquals(searchUrl, sessionManager.selectedSession?.url)
        assertEquals(searchTerms, sessionManager.selectedSession?.searchTerms)
    }

    @Test
    fun `processor handles ACTION_WEB_SEARCH with empty text`() {
        val handler = TabIntentProcessor(sessionManager, sessionUseCases.loadUrl, searchUseCases.newTabSearch)

        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_WEB_SEARCH)
        whenever(intent.getStringExtra(SearchManager.QUERY)).thenReturn(" ")

        val processed = handler.process(intent)
        assertFalse(processed)
    }

    @Test
    fun `load URL on ACTION_WEB_SEARCH if text is an URL`() {
        val handler = TabIntentProcessor(sessionManager, sessionUseCases.loadUrl, searchUseCases.newTabSearch)

        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_WEB_SEARCH)

        whenever(intent.getStringExtra(SearchManager.QUERY)).thenReturn("http://mozilla.org")
        handler.process(intent)
        val sessionCaptor = argumentCaptor<Session>()
        verify(sessionManager).add(sessionCaptor.capture(), eq(true), eq(null), eq(null), eq(null))
        verify(store).dispatch(EngineAction.LoadUrlAction(
            sessionCaptor.value.id, "http://mozilla.org", LoadUrlFlags.external())
        )
    }

    @Test
    fun `perform search on ACTION_WEB_SEARCH if text (no URL) provided`() {
        val engine = mock<Engine>()
        val sessionManager = spy(SessionManager(engine))

        val searchUseCases = SearchUseCases(testContext, store, searchEngineManager, sessionManager)
        val sessionUseCases = SessionUseCases(store, sessionManager)

        val searchTerms = "mozilla android"
        val searchUrl = "http://search-url.com?$searchTerms"

        val handler = TabIntentProcessor(sessionManager, sessionUseCases.loadUrl, searchUseCases.newTabSearch)

        val intent = mock<Intent>()
        whenever(intent.action).thenReturn(Intent.ACTION_WEB_SEARCH)
        whenever(intent.getStringExtra(SearchManager.QUERY)).thenReturn(searchTerms)

        val searchEngine = mock<SearchEngine>()
        whenever(searchEngine.buildSearchUrl(searchTerms)).thenReturn(searchUrl)
        whenever(searchEngineManager.getDefaultSearchEngine(testContext)).thenReturn(searchEngine)

        handler.process(intent)
        val sessionCaptor = argumentCaptor<Session>()
        verify(sessionManager).add(sessionCaptor.capture(), eq(true), eq(null), eq(null), eq(null))
        verify(store).dispatch(EngineAction.LoadUrlAction(
            sessionCaptor.value.id, searchUrl, LoadUrlFlags.none())
        )
        assertEquals(searchUrl, sessionManager.selectedSession?.url)
        assertEquals(searchTerms, sessionManager.selectedSession?.searchTerms)
    }
}
