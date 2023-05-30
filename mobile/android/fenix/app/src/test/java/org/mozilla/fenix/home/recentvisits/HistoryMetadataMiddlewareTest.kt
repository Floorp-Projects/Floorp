/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentvisits

import io.mockk.Called
import io.mockk.every
import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.HistoryMetadataAction
import mozilla.components.browser.state.action.MediaSessionAction
import mozilla.components.browser.state.action.SearchAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.engine.EngineMiddleware
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.history.HistoryItem
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.historymetadata.HistoryMetadataMiddleware
import org.mozilla.fenix.historymetadata.HistoryMetadataService

@RunWith(FenixRobolectricTestRunner::class)
class HistoryMetadataMiddlewareTest {

    private lateinit var store: BrowserStore
    private lateinit var middleware: HistoryMetadataMiddleware
    private lateinit var service: HistoryMetadataService

    @Before
    fun setUp() {
        service = mockk(relaxed = true)
        middleware = HistoryMetadataMiddleware(service)
        store = BrowserStore(
            middleware = listOf(middleware) + EngineMiddleware.create(engine = mockk()),
            initialState = BrowserState(),
        )
    }

    @Test
    fun `GIVEN normal tab WHEN history is updated THEN meta data is also recorded`() {
        val tab = createTab("https://mozilla.org")

        val expectedKey = HistoryMetadataKey(url = tab.content.url)
        every { service.createMetadata(any(), any()) } returns expectedKey

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        verify(exactly = 1) { service.createMetadata(tab) }

        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, emptyList(), currentIndex = 0)).joinBlocking()
        val capturedTab = slot<TabSessionState>()
        verify(exactly = 1) { service.createMetadata(capture(capturedTab)) }

        // Not recording if url didn't change.
        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, emptyList(), currentIndex = 0)).joinBlocking()
        verify(exactly = 1) { service.createMetadata(capture(capturedTab)) }

        assertEquals(tab.id, capturedTab.captured.id)
        assertEquals(expectedKey, store.state.findTab(tab.id)?.historyMetadata)

        // Now, test that we'll record metadata for the same tab after url is changed.
        store.dispatch(ContentAction.UpdateUrlAction(tab.id, "https://firefox.com")).joinBlocking()
        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, emptyList(), currentIndex = 0)).joinBlocking()

        val capturedTabs = mutableListOf<TabSessionState>()
        verify(exactly = 2) { service.createMetadata(capture(capturedTabs)) }

        assertEquals(2, capturedTabs.size)

        capturedTabs[0].apply {
            assertEquals(tab.id, id)
        }

        assertEquals(expectedKey, store.state.findTab(tab.id)?.historyMetadata)
    }

    @Test
    fun `GIVEN normal tab has parent with session search terms WHEN history metadata is recorded THEN search terms and referrer url are provided`() {
        val parentTab = createTab("https://google.com?q=mozilla+website", searchTerms = "mozilla website")
        val tab = createTab("https://mozilla.org", parent = parentTab)
        store.dispatch(TabListAction.AddTabAction(parentTab)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()

        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, emptyList(), currentIndex = 0)).joinBlocking()
        verify {
            service.createMetadata(any(), eq("mozilla website"), eq("https://google.com?q=mozilla+website"))
        }
    }

    @Test
    fun `GIVEN normal tab has search results parent without session search terms WHEN history metadata is recorded THEN search terms and referrer url are provided`() {
        setupGoogleSearchEngine()

        val parentTab = createTab("https://google.com?q=mozilla+website")
        val tab = createTab("https://mozilla.org", parent = parentTab)
        store.dispatch(TabListAction.AddTabAction(parentTab)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()

        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, emptyList(), currentIndex = 0)).joinBlocking()
        verify {
            service.createMetadata(any(), eq("mozilla website"), eq("https://google.com?q=mozilla+website"))
        }
    }

    @Test
    fun `GIVEN normal tab is a search engine result page WHEN history metadata is recorded THEN search terms are provided`() {
        service = TestingMetadataService()
        middleware = HistoryMetadataMiddleware(service)
        val tab = createTab("about:blank")
        store = BrowserStore(
            middleware = listOf(middleware) + EngineMiddleware.create(engine = mockk()),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )
        setupGoogleSearchEngine()

        val serpUrl = "https://google.com?q=mozilla+website"
        store.dispatch(EngineAction.LoadUrlAction(tab.id, serpUrl)).joinBlocking()
        store.dispatch(ContentAction.UpdateUrlAction(tab.id, serpUrl)).joinBlocking()
        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, listOf(HistoryItem("Google Search", serpUrl)), currentIndex = 0)).joinBlocking()
        with((service as TestingMetadataService).createdMetadata) {
            assertEquals(1, this.count())
            assertEquals("https://google.com?q=mozilla+website", this[0].url)
            assertEquals("mozilla website", this[0].searchTerm)
            assertNull(this[0].referrerUrl)
        }
    }

    @Test
    fun `GIVEN normal tab navigates to search engine result page WHEN history metadata is recorded THEN search terms are provided`() {
        service = TestingMetadataService()
        middleware = HistoryMetadataMiddleware(service)
        val tab = createTab("https://google.com")
        store = BrowserStore(
            middleware = listOf(middleware) + EngineMiddleware.create(engine = mockk()),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )
        setupGoogleSearchEngine()

        val serpUrl = "https://google.com?q=mozilla+website"
        store.dispatch(ContentAction.UpdateUrlAction(tab.id, serpUrl)).joinBlocking()
        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, listOf(HistoryItem("Google Search", "https://google.com"), HistoryItem("Google Search", serpUrl)), currentIndex = 1)).joinBlocking()
        with((service as TestingMetadataService).createdMetadata) {
            assertEquals(1, this.count())
            assertEquals("https://google.com?q=mozilla+website", this[0].url)
            assertEquals("mozilla website", this[0].searchTerm)
            assertNull(this[0].referrerUrl)
        }
    }

    @Test
    fun `GIVEN tab opened as new tab from a search page WHEN search page navigates away THEN redirecting or navigating in tab retains original search terms`() {
        service = TestingMetadataService()
        middleware = HistoryMetadataMiddleware(service)
        store = BrowserStore(
            middleware = listOf(middleware) + EngineMiddleware.create(engine = mockk()),
            initialState = BrowserState(),
        )
        setupGoogleSearchEngine()

        val parentTab = createTab("https://google.com?q=mozilla+website", searchTerms = "mozilla website")
        val tab = createTab("https://google.com?url=https://mozilla.org", parent = parentTab)
        store.dispatch(TabListAction.AddTabAction(parentTab, select = true)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()

        with((service as TestingMetadataService).createdMetadata) {
            assertEquals(2, this.count())
            assertEquals("https://google.com?q=mozilla+website", this[0].url)
            assertEquals("mozilla website", this[0].searchTerm)
            assertNull(this[0].referrerUrl)

            assertEquals("https://google.com?url=https://mozilla.org", this[1].url)
            assertEquals("mozilla website", this[1].searchTerm)
            assertEquals("https://google.com?q=mozilla+website", this[1].referrerUrl)
        }

        // Both tabs load.
        store.dispatch(ContentAction.UpdateHistoryStateAction(parentTab.id, listOf(HistoryItem("Google - mozilla website", "https://google.com?q=mozilla+website")), 0)).joinBlocking()
        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, listOf(HistoryItem("", "https://google.com?url=mozilla+website")), currentIndex = 0)).joinBlocking()
        with((service as TestingMetadataService).createdMetadata) {
            assertEquals(2, this.count())
        }

        // Parent navigates away.
        store.dispatch(ContentAction.UpdateUrlAction(parentTab.id, "https://firefox.com")).joinBlocking()
        store.dispatch(ContentAction.UpdateSearchTermsAction(parentTab.id, "")).joinBlocking()
        store.dispatch(ContentAction.UpdateHistoryStateAction(parentTab.id, listOf(HistoryItem("Google - mozilla website", "https://google.com?q=mozilla+website"), HistoryItem("Firefox", "https://firefox.com")), 1)).joinBlocking()
        with((service as TestingMetadataService).createdMetadata) {
            assertEquals(3, this.count())
            assertEquals("https://firefox.com", this[2].url)
            assertEquals("mozilla website", this[2].searchTerm)
            assertEquals("https://google.com?q=mozilla+website", this[2].referrerUrl)
        }

        // Redirect the child tab (url changed, history stack has single item).
        store.dispatch(ContentAction.UpdateUrlAction(tab.id, "https://mozilla.org")).joinBlocking()
        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, listOf(HistoryItem("Mozilla", "https://mozilla.org")), currentIndex = 0)).joinBlocking()
        val tab2 = store.state.findTab(tab.id)!!
        assertEquals("https://mozilla.org", tab2.content.url)
        with((service as TestingMetadataService).createdMetadata) {
            assertEquals(4, this.count())
            assertEquals("https://mozilla.org", this[3].url)
            assertEquals("mozilla website", this[3].searchTerm)
            assertEquals("https://google.com?q=mozilla+website", this[3].referrerUrl)
        }

        // Navigate the child tab.
        store.dispatch(ContentAction.UpdateUrlAction(tab.id, "https://mozilla.org/manifesto")).joinBlocking()
        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, listOf(HistoryItem("Mozilla", "https://mozilla.org"), HistoryItem("Mozilla Manifesto", "https://mozilla.org/manifesto")), currentIndex = 1)).joinBlocking()
        val tab3 = store.state.findTab(tab.id)!!
        assertEquals("https://mozilla.org/manifesto", tab3.content.url)

        with((service as TestingMetadataService).createdMetadata) {
            assertEquals(5, this.count())
            assertEquals("https://mozilla.org/manifesto", this[4].url)
            assertEquals("mozilla website", this[4].searchTerm)
            assertEquals("https://mozilla.org", this[4].referrerUrl)
        }
    }

    @Test
    fun `GIVEN tab opened as new tab from a search page WHEN it loads while parent navigates to a result THEN parent will retain its search terms`() {
        service = TestingMetadataService()
        middleware = HistoryMetadataMiddleware(service)
        store = BrowserStore(
            middleware = listOf(middleware) + EngineMiddleware.create(engine = mockk()),
            initialState = BrowserState(),
        )
        setupGoogleSearchEngine()

        val parentTab = createTab("https://google.com?q=mozilla+website", searchTerms = "mozilla website")
        val tab = createTab("https://google.com?url=https://mozilla.org", parent = parentTab)
        store.dispatch(TabListAction.AddTabAction(parentTab, select = true)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()

        with((service as TestingMetadataService).createdMetadata) {
            assertEquals(2, this.count())
            assertEquals("https://google.com?q=mozilla+website", this[0].url)
            assertEquals("mozilla website", this[0].searchTerm)
            assertNull(this[0].referrerUrl)

            assertEquals("https://google.com?url=https://mozilla.org", this[1].url)
            assertEquals("mozilla website", this[1].searchTerm)
            assertEquals("https://google.com?q=mozilla+website", this[1].referrerUrl)
        }

        // Parent tab loads.
        store.dispatch(ContentAction.UpdateHistoryStateAction(parentTab.id, listOf(HistoryItem("Google - mozilla website", "https://google.com?q=mozilla+website")), 0)).joinBlocking()
        with((service as TestingMetadataService).createdMetadata) {
            assertEquals(2, this.count())
        }

        // Simulate a state where search metadata is missing for the child tab.
        store.dispatch(HistoryMetadataAction.SetHistoryMetadataKeyAction(tab.id, HistoryMetadataKey("https://google.com?url=https://mozilla.org", null, null))).joinBlocking()

        // Parent navigates away, while the child starts loading. A mostly realistic sequence of events...
        store.dispatch(ContentAction.UpdateUrlAction(parentTab.id, "https://firefox.com")).joinBlocking()
        store.dispatch(ContentAction.UpdateSearchTermsAction(parentTab.id, "")).joinBlocking()
        store.dispatch(EngineAction.LoadUrlAction(tab.id, "https://google.com?url=https://mozilla.org")).joinBlocking()
        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, listOf(HistoryItem("", "https://google.com?url=https://mozilla.org")), currentIndex = 0)).joinBlocking()
        store.dispatch(ContentAction.UpdateUrlAction(tab.id, "https://mozilla.org")).joinBlocking()
        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, listOf(HistoryItem("Mozilla", "https://mozilla.org")), currentIndex = 0)).joinBlocking()
        store.dispatch(ContentAction.UpdateHistoryStateAction(parentTab.id, listOf(HistoryItem("Google - mozilla website", "https://google.com?q=mozilla+website"), HistoryItem("Firefox", "https://firefox.com")), 1)).joinBlocking()

        with((service as TestingMetadataService).createdMetadata) {
            assertEquals(4, this.count())
            assertEquals("https://firefox.com", this[3].url)
            assertEquals("mozilla website", this[3].searchTerm)
            assertEquals("https://google.com?q=mozilla+website", this[3].referrerUrl)

            assertEquals("https://google.com?url=https://mozilla.org", this[1].url)
            assertEquals("mozilla website", this[1].searchTerm)
            assertEquals("https://google.com?q=mozilla+website", this[1].referrerUrl)

            assertEquals("https://mozilla.org", this[2].url)
            assertEquals("mozilla website", this[2].searchTerm)
            // This is suspect. The parent tab switched away right before the child loaded, so the
            // referrer here is potentially bogus.
            assertEquals("https://firefox.com", this[2].referrerUrl)
        }
    }

    @Test
    fun `GIVEN tab with search terms WHEN subsequent direct load occurs THEN search terms are not retained`() {
        service = TestingMetadataService()
        middleware = HistoryMetadataMiddleware(service)
        store = BrowserStore(
            middleware = listOf(middleware) + EngineMiddleware.create(engine = mockk()),
            initialState = BrowserState(),
        )
        setupGoogleSearchEngine()

        val parentTab = createTab("https://google.com?q=mozilla+website", searchTerms = "mozilla website")
        val tab = createTab("https://google.com?url=https://mozilla.org", parent = parentTab)
        store.dispatch(TabListAction.AddTabAction(parentTab, select = true)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()

        with((service as TestingMetadataService).createdMetadata) {
            assertEquals(2, this.count())
            assertEquals("https://google.com?q=mozilla+website", this[0].url)
            assertEquals("mozilla website", this[0].searchTerm)
            assertNull(this[0].referrerUrl)

            assertEquals("https://google.com?url=https://mozilla.org", this[1].url)
            assertEquals("mozilla website", this[1].searchTerm)
            assertEquals("https://google.com?q=mozilla+website", this[1].referrerUrl)
        }

        // Both tabs load.
        store.dispatch(ContentAction.UpdateHistoryStateAction(parentTab.id, listOf(HistoryItem("Google - mozilla website", "https://google.com?q=mozilla+website")), 0)).joinBlocking()
        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, listOf(HistoryItem("", "https://google.com?url=mozilla+website")), currentIndex = 0)).joinBlocking()
        with((service as TestingMetadataService).createdMetadata) {
            assertEquals(2, this.count())
        }

        // Direct load occurs on child tab. Search terms should be cleared.
        store.dispatch(EngineAction.LoadUrlAction(tab.id, "https://firefox.com")).joinBlocking()
        store.dispatch(ContentAction.UpdateUrlAction(tab.id, "https://firefox.com")).joinBlocking()
        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, listOf(HistoryItem("", "https://google.com?url=mozilla+website"), HistoryItem("Firefox", "https://firefox.com")), 1)).joinBlocking()
        with((service as TestingMetadataService).createdMetadata) {
            assertEquals(3, this.count())
            assertEquals("https://firefox.com", this[2].url)
            assertNull(this[2].searchTerm)
            assertNull(this[2].referrerUrl)
        }

        // Direct load occurs on parent tab. Search terms should be cleared.
        store.dispatch(EngineAction.LoadUrlAction(parentTab.id, "https://firefox.com")).joinBlocking()
        store.dispatch(ContentAction.UpdateSearchTermsAction(parentTab.id, "")).joinBlocking()
        store.dispatch(ContentAction.UpdateUrlAction(parentTab.id, "https://firefox.com")).joinBlocking()
        store.dispatch(ContentAction.UpdateHistoryStateAction(parentTab.id, listOf(HistoryItem("Google - mozilla website", "https://google.com?q=mozilla+website"), HistoryItem("Firefox", "https://firefox.com")), 1)).joinBlocking()
        with((service as TestingMetadataService).createdMetadata) {
            assertEquals(4, this.count())
            assertEquals("https://firefox.com", this[3].url)
            assertNull(this[3].searchTerm)
            assertNull(this[3].referrerUrl)
        }
    }

    @Test
    fun `GIVEN tab with search terms WHEN subsequent optimized direct load occurs THEN search terms are not retained`() {
        service = TestingMetadataService()
        middleware = HistoryMetadataMiddleware(service)
        store = BrowserStore(
            middleware = listOf(middleware) + EngineMiddleware.create(engine = mockk()),
            initialState = BrowserState(),
        )
        setupGoogleSearchEngine()

        val parentTab = createTab("https://google.com?q=mozilla+website", searchTerms = "mozilla website")
        val tab = createTab("https://google.com?url=https://mozilla.org", parent = parentTab)
        store.dispatch(TabListAction.AddTabAction(parentTab, select = true)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()

        with((service as TestingMetadataService).createdMetadata) {
            assertEquals(2, this.count())
            assertEquals("https://google.com?q=mozilla+website", this[0].url)
            assertEquals("mozilla website", this[0].searchTerm)
            assertNull(this[0].referrerUrl)

            assertEquals("https://google.com?url=https://mozilla.org", this[1].url)
            assertEquals("mozilla website", this[1].searchTerm)
            assertEquals("https://google.com?q=mozilla+website", this[1].referrerUrl)
        }

        // Both tabs load.
        store.dispatch(ContentAction.UpdateHistoryStateAction(parentTab.id, listOf(HistoryItem("Google - mozilla website", "https://google.com?q=mozilla+website")), 0)).joinBlocking()
        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, listOf(HistoryItem("", "https://google.com?url=mozilla+website")), currentIndex = 0)).joinBlocking()
        with((service as TestingMetadataService).createdMetadata) {
            assertEquals(2, this.count())
        }

        // Direct load occurs on child tab. Search terms should be cleared.
        store.dispatch(EngineAction.OptimizedLoadUrlTriggeredAction(tab.id, "https://firefox.com")).joinBlocking()
        store.dispatch(ContentAction.UpdateUrlAction(tab.id, "https://firefox.com")).joinBlocking()
        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, listOf(HistoryItem("", "https://google.com?url=mozilla+website"), HistoryItem("Firefox", "https://firefox.com")), 1)).joinBlocking()
        with((service as TestingMetadataService).createdMetadata) {
            assertEquals(3, this.count())
            assertEquals("https://firefox.com", this[2].url)
            assertNull(this[2].searchTerm)
            assertNull(this[2].referrerUrl)
        }

        // Direct load occurs on parent tab. Search terms should be cleared.
        store.dispatch(EngineAction.OptimizedLoadUrlTriggeredAction(parentTab.id, "https://firefox.com")).joinBlocking()
        store.dispatch(ContentAction.UpdateSearchTermsAction(parentTab.id, "")).joinBlocking()
        store.dispatch(ContentAction.UpdateUrlAction(parentTab.id, "https://firefox.com")).joinBlocking()
        store.dispatch(ContentAction.UpdateHistoryStateAction(parentTab.id, listOf(HistoryItem("Google - mozilla website", "https://google.com?q=mozilla+website"), HistoryItem("Firefox", "https://firefox.com")), 1)).joinBlocking()
        with((service as TestingMetadataService).createdMetadata) {
            assertEquals(4, this.count())
            assertEquals("https://firefox.com", this[3].url)
            assertNull(this[3].searchTerm)
            assertNull(this[3].referrerUrl)
        }
    }

    @Test
    fun `GIVEN normal tab has parent WHEN url is the same THEN nothing happens`() {
        val parentTab = createTab("https://mozilla.org")
        val tab = createTab("https://mozilla.org", parent = parentTab)
        store.dispatch(TabListAction.AddTabAction(parentTab)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()

        verify(exactly = 1) { service.createMetadata(parentTab, null, null) }
        // Without our referrer url check, we would have recorded this metadata.
        verify(exactly = 0) { service.createMetadata(tab, "mozilla website", "https://mozilla.org") }
        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, emptyList(), currentIndex = 0)).joinBlocking()
        verify(exactly = 1) { service.createMetadata(any(), any(), any()) }
    }

    @Test
    fun `GIVEN normal tab has no parent WHEN history metadata is recorded THEN search terms and referrer url are provided`() {
        val tab = createTab("https://mozilla.org")
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        setupGoogleSearchEngine()

        val historyState = listOf(
            HistoryItem("firefox", "https://google.com?q=mozilla+website"),
            HistoryItem("mozilla", "https://mozilla.org"),
        )
        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, historyState, currentIndex = 1)).joinBlocking()

        verify {
            service.createMetadata(any(), eq("mozilla website"), eq("https://google.com?q=mozilla+website"))
        }
    }

    @Test
    fun `GIVEN normal tab has no parent WHEN history metadata is recorded without search terms THEN no referrer is provided`() {
        val tab = createTab("https://mozilla.org")
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        setupGoogleSearchEngine()

        val historyState = listOf(
            HistoryItem("firefox", "https://mozilla.org"),
            HistoryItem("mozilla", "https://firefox.com"),
        )
        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, historyState, currentIndex = 1)).joinBlocking()

        verify {
            service.createMetadata(any(), null, null)
        }
    }

    @Test
    fun `GIVEN a normal tab with history state WHEN directly loaded THEN search terms and referrer not recorded`() {
        val tab = createTab("https://mozilla.org")
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        setupGoogleSearchEngine()

        val historyState = listOf(
            HistoryItem("firefox", "https://google.com?q=mozilla+website"),
            HistoryItem("mozilla", "https://mozilla.org"),
        )
        store.dispatch(EngineAction.LoadUrlAction(tab.id, tab.content.url)).joinBlocking()
        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, historyState, currentIndex = 1)).joinBlocking()

        verify {
            service.createMetadata(any(), null, null)
        }

        // Once direct load is "consumed", we're looking up the history stack again.
        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, historyState, currentIndex = 1)).joinBlocking()
        verify {
            service.createMetadata(any(), eq("mozilla website"), eq("https://google.com?q=mozilla+website"))
        }
    }

    @Test
    fun `GIVEN private tab WHEN loading completed THEN no meta data is recorded`() {
        val tab = createTab("https://mozilla.org", private = true)

        val expectedKey = HistoryMetadataKey(url = tab.content.url)
        every { service.createMetadata(any(), any()) } returns expectedKey

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        verify { service wasNot Called }

        store.dispatch(ContentAction.UpdateHistoryStateAction(tab.id, emptyList(), currentIndex = 0)).joinBlocking()
        verify { service wasNot Called }
    }

    @Test
    fun `GIVEN normal tab WHEN update url action event with a different url is received THEN meta data is updated`() {
        val existingKey = HistoryMetadataKey(url = "https://mozilla.org")
        val tab = createTab(url = existingKey.url, historyMetadata = existingKey)

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        verify { service wasNot Called }

        store.dispatch(ContentAction.UpdateUrlAction(tab.id, "https://www.someother.url")).joinBlocking()
        val capturedTab = slot<TabSessionState>()
        verify { service.updateMetadata(existingKey, capture(capturedTab)) }

        assertEquals(tab.id, capturedTab.captured.id)
    }

    @Test
    fun `GIVEN normal tab WHEN update url action event with the same url is received THEN meta data is not updated`() {
        val existingKey = HistoryMetadataKey(url = "https://mozilla.org")
        val tab = createTab(url = existingKey.url, historyMetadata = existingKey)

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        verify { service wasNot Called }

        store.dispatch(ContentAction.UpdateUrlAction(tab.id, existingKey.url)).joinBlocking()
        verify { service wasNot Called }
    }

    @Test
    fun `GIVEN tab without metadata WHEN user navigates and new page starts loading THEN nothing happens`() {
        val tab = createTab(url = "https://mozilla.org")

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        verify(exactly = 1) { service.createMetadata(tab) }

        store.dispatch(ContentAction.UpdateLoadingStateAction(tab.id, true)).joinBlocking()
        verify(exactly = 1) { service.createMetadata(any()) }
        verify(exactly = 0) { service.updateMetadata(any(), any()) }
    }

    @Test
    fun `GIVEN tab is not selected WHEN user navigates and new page starts loading THEN nothing happens`() {
        val existingKey = HistoryMetadataKey(url = "https://mozilla.org")
        val tab = createTab(url = "https://mozilla.org", historyMetadata = existingKey)
        val otherTab = createTab(url = "https://blog.mozilla.org")

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(otherTab, select = true)).joinBlocking()
        val capturedTab = slot<TabSessionState>()
        verify(exactly = 1) { service.updateMetadata(existingKey, capture(capturedTab)) }
        assertEquals(tab.id, capturedTab.captured.id)

        store.dispatch(ContentAction.UpdateLoadingStateAction(tab.id, true)).joinBlocking()
        verify(exactly = 1) { service.updateMetadata(existingKey, capture(capturedTab)) }
    }

    @Test
    fun `GIVEN normal media tab WHEN media state is updated THEN meta data is recorded`() {
        val tab = createTab("https://media.mozilla.org")

        val expectedKey = HistoryMetadataKey(url = tab.content.url)
        every { service.createMetadata(any(), any()) } returns expectedKey

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        val capturedTabs = mutableListOf<TabSessionState>()
        verify {
            service.createMetadata(capture(capturedTabs), null, null)
        }
        assertEquals(tab.id, capturedTabs[0].id)
        assertNull(capturedTabs[0].historyMetadata)
        assertEquals(expectedKey, store.state.findTab(tab.id)?.historyMetadata)

        store.dispatch(MediaSessionAction.UpdateMediaMetadataAction(tab.id, mockk())).joinBlocking()
        verify {
            service.createMetadata(capture(capturedTabs), null, null)
        }

        // Ugh, why are there three captured tabs when only two invocations of createMetadata happened?
        assertEquals(tab.id, capturedTabs[2].id)
        assertEquals(expectedKey, capturedTabs[2].historyMetadata)
        assertEquals(expectedKey, store.state.findTab(tab.id)?.historyMetadata)
    }

    @Test
    fun `GIVEN private media tab WHEN media state is updated THEN no meta data is recorded`() {
        val tab = createTab("https://media.mozilla.org", private = true)

        val expectedKey = HistoryMetadataKey(url = tab.content.url)
        every { service.createMetadata(any(), any()) } returns expectedKey

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        verify { service wasNot Called }

        store.dispatch(MediaSessionAction.UpdateMediaMetadataAction(tab.id, mockk())).joinBlocking()
        verify { service wasNot Called }
    }

    @Test
    fun `GIVEN normal tab is selected WHEN new tab will be added and selected THEN meta data is updated for currently selected tab`() {
        val existingKey = HistoryMetadataKey(url = "https://mozilla.org")
        val tab = createTab(url = "https://mozilla.org", historyMetadata = existingKey)
        val otherTab = createTab(url = "https://blog.mozilla.org")
        val yetAnotherTab = createTab(url = "https://media.mozilla.org")

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(otherTab)).joinBlocking()
        verify(exactly = 1) { service.createMetadata(any()) }
        verify(exactly = 0) { service.updateMetadata(any(), any()) }

        store.dispatch(TabListAction.AddTabAction(yetAnotherTab, select = true)).joinBlocking()
        val capturedTab = slot<TabSessionState>()
        verify(exactly = 2) { service.createMetadata(any()) }
        verify(exactly = 1) { service.updateMetadata(existingKey, capture(capturedTab)) }
        assertEquals(tab.id, capturedTab.captured.id)
    }

    @Test
    fun `GIVEN private tab is selected WHEN new tab will be added and selected THEN metadata not updated for private tab`() {
        val existingKey = HistoryMetadataKey(url = "https://mozilla.org")
        val tab = createTab(url = "https://mozilla.org", historyMetadata = existingKey, private = true)
        val otherTab = createTab(url = "https://blog.mozilla.org")
        val yetAnotherTab = createTab(url = "https://media.mozilla.org")

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        verify { service wasNot Called }
        store.dispatch(TabListAction.AddTabAction(otherTab)).joinBlocking()
        verify { service.createMetadata(otherTab) }

        store.dispatch(TabListAction.AddTabAction(yetAnotherTab, select = true)).joinBlocking()
        verify { service.createMetadata(yetAnotherTab) }
    }

    @Test
    fun `GIVEN normal tab is selected WHEN new tab will be selected THEN meta data is updated for currently selected tab`() {
        val existingKey = HistoryMetadataKey(url = "https://mozilla.org")
        val tab = createTab(url = "https://mozilla.org", historyMetadata = existingKey)
        val otherTab = createTab(url = "https://blog.mozilla.org")

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        verify { service wasNot Called }

        store.dispatch(TabListAction.SelectTabAction(otherTab.id)).joinBlocking()
        val capturedTab = slot<TabSessionState>()
        verify(exactly = 1) { service.updateMetadata(existingKey, capture(capturedTab)) }
        assertEquals(tab.id, capturedTab.captured.id)
    }

    @Test
    fun `GIVEN private tab is selected WHEN new tab will be selected THEN nothing happens`() {
        val existingKey = HistoryMetadataKey(url = "https://mozilla.org")
        val tab = createTab(url = "https://mozilla.org", historyMetadata = existingKey, private = true)
        val otherTab = createTab(url = "https://blog.mozilla.org")

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        verify { service wasNot Called }

        store.dispatch(TabListAction.SelectTabAction(otherTab.id)).joinBlocking()
        verify { service wasNot Called }
    }

    @Test
    fun `WHEN normal selected tab is removed THEN meta data is updated`() {
        val existingKey = HistoryMetadataKey(url = "https://mozilla.org")
        val tab = createTab(url = "https://mozilla.org", historyMetadata = existingKey)

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        verify { service wasNot Called }

        store.dispatch(TabListAction.RemoveTabAction(tab.id)).joinBlocking()
        val capturedTab = slot<TabSessionState>()
        verify(exactly = 1) { service.updateMetadata(existingKey, capture(capturedTab)) }
        assertEquals(tab.id, capturedTab.captured.id)
    }

    @Test
    fun `WHEN private selected tab is removed THEN nothing happens`() {
        val existingKey = HistoryMetadataKey(url = "https://mozilla.org")
        val tab = createTab(url = "https://mozilla.org", historyMetadata = existingKey, private = true)

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        verify { service wasNot Called }

        store.dispatch(TabListAction.RemoveTabAction(tab.id)).joinBlocking()
        verify { service wasNot Called }
    }

    @Test
    fun `WHEN non-selected tab is removed THEN nothing happens`() {
        val existingKey = HistoryMetadataKey(url = "https://mozilla.org")
        val tab = createTab(url = "https://mozilla.org", historyMetadata = existingKey)
        val otherTab = createTab(url = "https://blog.mozilla.org")

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(otherTab)).joinBlocking()
        // 1 because 'tab' already has a metadata key set.
        verify(exactly = 1) { service.createMetadata(any()) }

        store.dispatch(TabListAction.RemoveTabAction(otherTab.id)).joinBlocking()
        verify(exactly = 1) { service.createMetadata(any()) }
    }

    @Test
    fun `GIVEN multiple tabs are removed WHEN selected normal tab should also be removed THEN meta data is updated`() {
        val existingKey = HistoryMetadataKey(url = "https://mozilla.org")
        val tab = createTab(url = "https://mozilla.org", historyMetadata = existingKey)
        val otherTab = createTab(url = "https://blog.mozilla.org")
        val yetAnotherTab = createTab(url = "https://media.mozilla.org")

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(otherTab)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(yetAnotherTab)).joinBlocking()
        // 'tab' already has an existing key, so metadata isn't created for it.
        verify(exactly = 2) { service.createMetadata(any()) }

        store.dispatch(TabListAction.RemoveTabsAction(listOf(tab.id, otherTab.id))).joinBlocking()
        val capturedTab = slot<TabSessionState>()
        verify(exactly = 1) { service.updateMetadata(existingKey, capture(capturedTab)) }
        assertEquals(tab.id, capturedTab.captured.id)
    }

    @Test
    fun `GIVEN multiple tabs are removed WHEN selected private tab should also be removed THEN nothing happens`() {
        val existingKey = HistoryMetadataKey(url = "https://mozilla.org")
        val tab = createTab(url = "https://mozilla.org", historyMetadata = existingKey, private = true)
        val otherTab = createTab(url = "https://blog.mozilla.org")
        val yetAnotherTab = createTab(url = "https://media.mozilla.org")

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        verify { service wasNot Called }
        store.dispatch(TabListAction.AddTabAction(otherTab)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(yetAnotherTab)).joinBlocking()
        verify(exactly = 2) { service.createMetadata(any()) }

        store.dispatch(TabListAction.RemoveTabsAction(listOf(tab.id, otherTab.id))).joinBlocking()
        verify(exactly = 2) { service.createMetadata(any()) }
        verify(exactly = 0) { service.updateMetadata(any(), any()) }
    }

    @Test
    fun `GIVEN multiple tabs are removed WHEN selected tab should not be removed THEN nothing happens`() {
        val existingKey = HistoryMetadataKey(url = "https://mozilla.org")
        val tab = createTab(url = "https://mozilla.org", historyMetadata = existingKey)
        val otherTab = createTab(url = "https://blog.mozilla.org")
        val yetAnotherTab = createTab(url = "https://media.mozilla.org")

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(otherTab)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(yetAnotherTab)).joinBlocking()
        verify(exactly = 2) { service.createMetadata(any()) }
        verify(exactly = 0) { service.updateMetadata(any(), any()) }

        store.dispatch(TabListAction.RemoveTabsAction(listOf(otherTab.id, yetAnotherTab.id))).joinBlocking()
        verify(exactly = 2) { service.createMetadata(any()) }
        verify(exactly = 0) { service.updateMetadata(any(), any()) }
    }

    private fun setupGoogleSearchEngine() {
        store.dispatch(
            SearchAction.SetSearchEnginesAction(
                regionSearchEngines = listOf(
                    SearchEngine(
                        id = "google",
                        name = "Google",
                        icon = mockk(),
                        type = SearchEngine.Type.BUNDLED,
                        resultUrls = listOf("https://google.com?q={searchTerms}"),
                    ),
                ),
                userSelectedSearchEngineId = null,
                userSelectedSearchEngineName = null,
                regionDefaultSearchEngineId = "google",
                customSearchEngines = emptyList(),
                hiddenSearchEngines = emptyList(),
                disabledSearchEngineIds = emptyList(),
                additionalAvailableSearchEngines = emptyList(),
                additionalSearchEngines = emptyList(),
                regionSearchEnginesOrder = listOf("google"),
            ),
        ).joinBlocking()
    }

    // Provides a more convenient way of capturing arguments for the functions we care about.
    // I.e. capturing arguments in mockk was driving me mad and this is easy to understand and works.
    class TestingMetadataService : HistoryMetadataService {
        val createdMetadata = mutableListOf<HistoryMetadataKey>()

        override fun createMetadata(
            tab: TabSessionState,
            searchTerms: String?,
            referrerUrl: String?,
        ): HistoryMetadataKey {
            createdMetadata.add(HistoryMetadataKey(tab.content.url, searchTerms, referrerUrl))
            return HistoryMetadataKey(tab.content.url, searchTerms, referrerUrl)
        }

        override fun updateMetadata(key: HistoryMetadataKey, tab: TabSessionState) {}
        override fun cleanup(olderThan: Long) {}
    }
}
