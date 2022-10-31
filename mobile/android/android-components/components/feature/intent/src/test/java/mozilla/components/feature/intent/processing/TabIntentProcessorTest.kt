/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.intent.processing

import android.app.SearchManager
import android.content.Intent
import android.nfc.NfcAdapter.ACTION_NDEF_DISCOVERED
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.engine.EngineMiddleware
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.selector.findNormalOrPrivateTabByUrl
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.search.ext.createSearchEngine
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyBoolean
import org.mockito.Mockito.anyString
import org.mockito.Mockito.doReturn

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class TabIntentProcessorTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    private lateinit var middleware: CaptureActionsMiddleware<BrowserState, BrowserAction>

    private lateinit var searchEngine: SearchEngine
    private lateinit var store: BrowserStore
    private lateinit var engine: Engine
    private lateinit var engineSession: EngineSession

    private lateinit var sessionUseCases: SessionUseCases
    private lateinit var tabsUseCases: TabsUseCases
    private lateinit var searchUseCases: SearchUseCases

    @Before
    fun setup() {
        searchEngine = createSearchEngine(
            name = "Test",
            url = "https://localhost/?q={searchTerms}",
            icon = mock(),
        )

        engine = mock()
        engineSession = mock()
        doReturn(engineSession).`when`(engine).createSession(anyBoolean(), anyString())

        middleware = CaptureActionsMiddleware()

        store = BrowserStore(
            BrowserState(
                search = SearchState(regionSearchEngines = listOf(searchEngine)),
            ),
            middleware = EngineMiddleware.create(
                engine = mock(),
                scope = scope,
            ) + listOf(middleware),
        )

        sessionUseCases = SessionUseCases(store)
        tabsUseCases = TabsUseCases(store)
        searchUseCases = SearchUseCases(store, tabsUseCases, sessionUseCases)
    }

    @Test
    fun `open or select tab on ACTION_VIEW intent`() {
        val handler = TabIntentProcessor(TabsUseCases(store), searchUseCases.newTabSearch)
        val intent: Intent = mock()
        whenever(intent.action).thenReturn(Intent.ACTION_VIEW)
        whenever(intent.dataString).thenReturn("http://mozilla.org")

        assertEquals(0, store.state.tabs.size)
        handler.process(intent)
        store.waitUntilIdle()
        assertEquals(1, store.state.tabs.size)
        assertTrue(store.state.tabs[0].source is SessionState.Source.External.ActionView)

        val tab = store.state.findNormalOrPrivateTabByUrl("http://mozilla.org", private = false)
        assertNotNull(tab)

        val otherTab = createTab("https://firefox.com")
        store.dispatch(TabListAction.AddTabAction(otherTab, select = true)).joinBlocking()
        assertEquals(2, store.state.tabs.size)
        assertEquals(otherTab, store.state.selectedTab)
        assertTrue(store.state.tabs[1].source is SessionState.Source.Internal.None)

        // processing the same intent again doesn't add an additional tab
        handler.process(intent)
        // processing a similar intent which produces the same url doesn't add an additional tab
        whenever(intent.dataString).thenReturn("mozilla.org")
        handler.process(intent)

        store.waitUntilIdle()
        assertEquals(2, store.state.tabs.size)
        assertEquals(tab, store.state.selectedTab)
        // sources of existing tabs weren't affected
        assertTrue(store.state.tabs[0].source is SessionState.Source.External.ActionView)
        assertTrue(store.state.tabs[1].source is SessionState.Source.Internal.None)

        // Intent with a url that's missing a scheme
        whenever(intent.dataString).thenReturn("example.com")
        handler.process(intent)
        store.waitUntilIdle()
        assertEquals(3, store.state.tabs.size)
        assertTrue(store.state.tabs[0].source is SessionState.Source.External.ActionView)
        assertNotNull(store.state.findNormalOrPrivateTabByUrl("http://example.com", private = false))
    }

    @Test
    fun `open or select tab on ACTION_MAIN intent`() {
        val handler = TabIntentProcessor(TabsUseCases(store), searchUseCases.newTabSearch)
        val intent: Intent = mock()
        whenever(intent.action).thenReturn(Intent.ACTION_MAIN)
        whenever(intent.dataString).thenReturn("https://mozilla.org")

        assertEquals(0, store.state.tabs.size)
        handler.process(intent)
        store.waitUntilIdle()
        assertEquals(1, store.state.tabs.size)
        assertTrue(store.state.tabs[0].source is SessionState.Source.External.ActionView)

        val tab = store.state.findNormalOrPrivateTabByUrl("https://mozilla.org", false)
        assertNotNull(tab)

        val otherTab = createTab("https://firefox.com")
        store.dispatch(TabListAction.AddTabAction(otherTab, select = true)).joinBlocking()
        assertEquals(2, store.state.tabs.size)
        assertEquals(otherTab, store.state.selectedTab)

        handler.process(intent)
        store.waitUntilIdle()
        assertEquals(2, store.state.tabs.size)
        assertEquals(tab, store.state.selectedTab)

        // Intent with a url that's missing a scheme
        whenever(intent.dataString).thenReturn("example.com")
        handler.process(intent)
        store.waitUntilIdle()
        assertEquals(3, store.state.tabs.size)
        assertTrue(store.state.tabs[0].source is SessionState.Source.External.ActionView)
        assertNotNull(store.state.findNormalOrPrivateTabByUrl("http://example.com", private = false))
    }

    @Test
    fun `open or select tab on ACTION_NDEF_DISCOVERED intent`() {
        val handler = TabIntentProcessor(TabsUseCases(store), searchUseCases.newTabSearch)
        val intent: Intent = mock()
        whenever(intent.action).thenReturn(ACTION_NDEF_DISCOVERED)
        whenever(intent.dataString).thenReturn("https://mozilla.org")

        assertEquals(0, store.state.tabs.size)
        handler.process(intent)
        store.waitUntilIdle()
        assertEquals(1, store.state.tabs.size)

        val tab = store.state.findNormalOrPrivateTabByUrl("https://mozilla.org", false)
        assertNotNull(tab)

        val otherTab = createTab("https://firefox.com")
        store.dispatch(TabListAction.AddTabAction(otherTab, select = true)).joinBlocking()
        assertEquals(2, store.state.tabs.size)
        assertEquals(otherTab, store.state.selectedTab)

        handler.process(intent)
        store.waitUntilIdle()
        assertEquals(2, store.state.tabs.size)
        assertEquals(tab, store.state.selectedTab)

        // Intent with a url that's missing a scheme
        whenever(intent.dataString).thenReturn("example.com")
        handler.process(intent)
        store.waitUntilIdle()
        assertEquals(3, store.state.tabs.size)
        assertTrue(store.state.tabs[0].source is SessionState.Source.External.ActionView)
        assertNotNull(store.state.findNormalOrPrivateTabByUrl("http://example.com", private = false))
    }

    @Test
    fun `open tab on ACTION_SEND intent`() {
        val handler = TabIntentProcessor(TabsUseCases(store), searchUseCases.newTabSearch)

        val intent: Intent = mock()
        whenever(intent.action).thenReturn(Intent.ACTION_SEND)
        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("https://mozilla.org")

        assertEquals(0, store.state.tabs.size)
        handler.process(intent)
        store.waitUntilIdle()
        assertEquals(1, store.state.tabs.size)
        assertEquals("https://mozilla.org", store.state.tabs[0].content.url)
        assertTrue(store.state.tabs[0].source is SessionState.Source.External.ActionSend)

        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("see https://getpocket.com")
        handler.process(intent)
        store.waitUntilIdle()
        assertEquals(2, store.state.tabs.size)
        assertEquals("https://getpocket.com", store.state.tabs[1].content.url)
        assertTrue(store.state.tabs[1].source is SessionState.Source.External.ActionSend)

        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("see https://firefox.com and https://mozilla.org")
        handler.process(intent)
        store.waitUntilIdle()
        assertEquals(3, store.state.tabs.size)
        assertEquals("https://firefox.com", store.state.tabs[2].content.url)
        assertTrue(store.state.tabs[2].source is SessionState.Source.External.ActionSend)

        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("checkout the Tweet: https://tweets.mozilla.com")
        handler.process(intent)
        store.waitUntilIdle()
        assertEquals(4, store.state.tabs.size)
        assertEquals("https://tweets.mozilla.com", store.state.tabs[3].content.url)
        assertTrue(store.state.tabs[3].source is SessionState.Source.External.ActionSend)

        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("checkout the Tweet: HTTPS://tweets.mozilla.org")
        handler.process(intent)
        store.waitUntilIdle()
        assertEquals(5, store.state.tabs.size)
        assertEquals("https://tweets.mozilla.org", store.state.tabs[4].content.url)
        assertTrue(store.state.tabs[4].source is SessionState.Source.External.ActionSend)

        // Intent with a url that's missing a scheme
        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn("example.com")
        handler.process(intent)
        store.waitUntilIdle()
        assertEquals(6, store.state.tabs.size)
        assertTrue(store.state.tabs[5].source is SessionState.Source.External.ActionSend)
        assertNotNull(store.state.findNormalOrPrivateTabByUrl("http://example.com", private = false))
    }

    @Test
    fun `open tab and trigger search on ACTION_SEND if text is not a URL`() {
        val handler = TabIntentProcessor(TabsUseCases(store), searchUseCases.newTabSearch)

        val searchTerms = "mozilla android"
        val searchUrl = "https://localhost/?q=mozilla%20android"

        val intent: Intent = mock()
        whenever(intent.action).thenReturn(Intent.ACTION_SEND)
        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn(searchTerms)

        assertEquals(0, store.state.tabs.size)
        handler.process(intent)

        store.waitUntilIdle()
        assertEquals(1, store.state.tabs.size)
        assertEquals(searchUrl, store.state.tabs[0].content.url)
        assertEquals(searchTerms, store.state.tabs[0].content.searchTerms)
        assertTrue(store.state.tabs[0].source is SessionState.Source.External.ActionSend)
    }

    @Test
    fun `nothing happens on ACTION_SEND if no text is provided`() {
        val handler = TabIntentProcessor(TabsUseCases(store), searchUseCases.newTabSearch)

        val intent: Intent = mock()
        whenever(intent.action).thenReturn(Intent.ACTION_SEND)
        whenever(intent.getStringExtra(Intent.EXTRA_TEXT)).thenReturn(" ")

        val processed = handler.process(intent)
        assertFalse(processed)
    }

    @Test
    fun `nothing happens on ACTION_SEARCH if text is empty`() {
        val handler = TabIntentProcessor(TabsUseCases(store), searchUseCases.newTabSearch)

        val intent: Intent = mock()
        whenever(intent.action).thenReturn(Intent.ACTION_SEARCH)
        whenever(intent.getStringExtra(SearchManager.QUERY)).thenReturn(" ")

        val processed = handler.process(intent)
        assertFalse(processed)
    }

    @Test
    fun `open tab on ACTION_SEARCH intent`() {
        val handler = TabIntentProcessor(TabsUseCases(store), searchUseCases.newTabSearch)

        val intent: Intent = mock()
        whenever(intent.action).thenReturn(Intent.ACTION_SEARCH)
        whenever(intent.getStringExtra(SearchManager.QUERY)).thenReturn("http://mozilla.org")

        assertEquals(0, store.state.tabs.size)
        handler.process(intent)

        store.waitUntilIdle()
        assertEquals(1, store.state.tabs.size)
        assertEquals("http://mozilla.org", store.state.tabs[0].content.url)
        assertEquals("", store.state.tabs[0].content.searchTerms)
        assertTrue(store.state.tabs[0].source is SessionState.Source.External.ActionSearch)

        // Intent with a url that's missing a scheme
        whenever(intent.getStringExtra(SearchManager.QUERY)).thenReturn("example.com")
        handler.process(intent)
        store.waitUntilIdle()
        assertEquals(2, store.state.tabs.size)
        assertTrue(store.state.tabs[1].source is SessionState.Source.External.ActionSearch)
        assertNotNull(store.state.findNormalOrPrivateTabByUrl("http://example.com", private = false))
    }

    @Test
    fun `open tab and trigger search on ACTION_SEARCH intent if text is not a URL`() {
        val handler = TabIntentProcessor(TabsUseCases(store), searchUseCases.newTabSearch)

        val searchTerms = "mozilla android"
        val searchUrl = "https://localhost/?q=mozilla%20android"

        val intent: Intent = mock()
        whenever(intent.action).thenReturn(Intent.ACTION_SEARCH)
        whenever(intent.getStringExtra(SearchManager.QUERY)).thenReturn(searchTerms)

        assertEquals(0, store.state.tabs.size)
        handler.process(intent)

        store.waitUntilIdle()
        assertEquals(1, store.state.tabs.size)
        assertEquals(searchUrl, store.state.tabs[0].content.url)
        assertEquals(searchTerms, store.state.tabs[0].content.searchTerms)
        assertTrue(store.state.tabs[0].source is SessionState.Source.External.ActionSearch)
    }

    @Test
    fun `nothing happens on ACTION_WEB_SEARCH if text is empty`() {
        val handler = TabIntentProcessor(TabsUseCases(store), searchUseCases.newTabSearch)

        val intent: Intent = mock()
        whenever(intent.action).thenReturn(Intent.ACTION_WEB_SEARCH)
        whenever(intent.getStringExtra(SearchManager.QUERY)).thenReturn(" ")

        val processed = handler.process(intent)
        assertFalse(processed)
    }

    @Test
    fun `open tab on ACTION_WEB_SEARCH intent`() {
        val handler = TabIntentProcessor(TabsUseCases(store), searchUseCases.newTabSearch)

        val intent: Intent = mock()
        whenever(intent.action).thenReturn(Intent.ACTION_WEB_SEARCH)
        whenever(intent.getStringExtra(SearchManager.QUERY)).thenReturn("http://mozilla.org")

        assertEquals(0, store.state.tabs.size)
        handler.process(intent)
        store.waitUntilIdle()
        assertEquals(1, store.state.tabs.size)
        assertEquals("http://mozilla.org", store.state.tabs[0].content.url)
        assertEquals("", store.state.tabs[0].content.searchTerms)
        assertTrue(store.state.tabs[0].source is SessionState.Source.External.ActionSearch)

        // Intent with a url that's missing a scheme
        whenever(intent.getStringExtra(SearchManager.QUERY)).thenReturn("example.com")
        handler.process(intent)
        store.waitUntilIdle()
        assertEquals(2, store.state.tabs.size)
        assertTrue(store.state.tabs[1].source is SessionState.Source.External.ActionSearch)
        assertNotNull(store.state.findNormalOrPrivateTabByUrl("http://example.com", private = false))
    }

    @Test
    fun `open tab and trigger search on ACTION_WEB_SEARCH intent if text is not a URL`() {
        val handler = TabIntentProcessor(TabsUseCases(store), searchUseCases.newTabSearch)

        val searchTerms = "mozilla android"
        val searchUrl = "https://localhost/?q=mozilla%20android"

        val intent: Intent = mock()
        whenever(intent.action).thenReturn(Intent.ACTION_SEARCH)
        whenever(intent.getStringExtra(SearchManager.QUERY)).thenReturn(searchTerms)

        assertEquals(0, store.state.tabs.size)
        handler.process(intent)
        store.waitUntilIdle()
        assertEquals(1, store.state.tabs.size)
        assertEquals(searchUrl, store.state.tabs[0].content.url)
        assertEquals(searchTerms, store.state.tabs[0].content.searchTerms)
        assertTrue(store.state.tabs[0].source is SessionState.Source.External.ActionSearch)
    }
}
