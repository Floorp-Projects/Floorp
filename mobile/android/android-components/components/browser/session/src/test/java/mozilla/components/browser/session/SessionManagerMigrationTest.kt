/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import mozilla.components.browser.session.ext.toFindResultState
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.support.base.observer.Consumable
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.`when`

/**
 * This test suite validates that calls on [SessionManager] update [BrowserStore] to create a matching state.
 */
class SessionManagerMigrationTest {
    @Test
    fun `Add session`() {
        val store = BrowserStore()

        val sessionManager = SessionManager(engine = mock(), store = store)

        assertTrue(sessionManager.sessions.isEmpty())
        assertTrue(store.state.tabs.isEmpty())

        sessionManager.add(Session("https://www.mozilla.org", private = true))

        assertEquals(1, sessionManager.sessions.size)
        assertEquals(1, store.state.tabs.size)

        val tab = store.state.tabs[0]

        assertEquals("https://www.mozilla.org", tab.content.url)
        assertTrue(tab.content.private)
    }

    @Test
    fun `Add custom tab session`() {
        val store = BrowserStore()

        val sessionManager = SessionManager(engine = mock(), store = store)

        assertTrue(sessionManager.sessions.isEmpty())
        assertTrue(store.state.tabs.isEmpty())

        val customTabSession = Session("https://www.mozilla.org")
        customTabSession.customTabConfig = mock()
        sessionManager.add(customTabSession)

        assertEquals(0, sessionManager.sessions.size)
        assertEquals(1, sessionManager.all.size)
        assertEquals(0, store.state.tabs.size)
        assertEquals(1, store.state.customTabs.size)

        val tab = store.state.customTabs[0]
        assertEquals("https://www.mozilla.org", tab.content.url)
    }

    @Test
    fun `Migrating a custom tab session`() {
        val store = BrowserStore()

        val sessionManager = SessionManager(engine = mock(), store = store)

        val customTabSession = Session("https://www.mozilla.org")
        customTabSession.customTabConfig = mock()
        sessionManager.add(customTabSession)

        assertEquals(0, sessionManager.sessions.size)
        assertEquals(1, sessionManager.all.size)
        assertEquals(0, store.state.tabs.size)
        assertEquals(1, store.state.customTabs.size)

        val customTab = store.state.customTabs[0]
        assertEquals("https://www.mozilla.org", customTab.content.url)

        customTabSession.customTabConfig = null
        assertEquals(1, sessionManager.sessions.size)
        assertEquals(1, sessionManager.all.size)
        assertEquals(1, store.state.tabs.size)
        assertEquals(0, store.state.customTabs.size)

        val tab = store.state.tabs[0]
        assertEquals("https://www.mozilla.org", tab.content.url)
    }

    @Test
    fun `Remove session`() {
        val store = BrowserStore()

        val sessionManager = SessionManager(engine = mock(), store = store)

        sessionManager.add(Session("https://www.mozilla.org"))
        sessionManager.add(Session("https://www.firefox.com"))

        assertEquals(2, sessionManager.sessions.size)
        assertEquals(2, store.state.tabs.size)

        sessionManager.remove(sessionManager.sessions[0])

        assertEquals(1, sessionManager.sessions.size)
        assertEquals(1, store.state.tabs.size)

        assertEquals("https://www.firefox.com", store.state.tabs[0].content.url)
    }

    @Test
    fun `Remove all regular sessions`() {
        val store = BrowserStore()

        val sessionManager = SessionManager(engine = mock(), store = store)
        sessionManager.add(Session("https://www.mozilla.org"))
        sessionManager.add(Session("https://www.firefox.com"))
        sessionManager.add(Session("https://www.getpocket.com").apply { customTabConfig = mock() })

        assertEquals(2, sessionManager.sessions.size)
        assertEquals(3, sessionManager.all.size)
        assertEquals(2, store.state.tabs.size)
        assertEquals(1, store.state.customTabs.size)

        sessionManager.removeSessions()

        assertEquals(0, sessionManager.sessions.size)
        assertEquals(1, sessionManager.all.size)
        assertEquals(0, store.state.tabs.size)
        assertEquals(1, store.state.customTabs.size)
    }

    @Test
    fun `Remove all custom tab and regular sessions`() {
        val store = BrowserStore()

        val sessionManager = SessionManager(engine = mock(), store = store)
        sessionManager.add(Session("https://www.mozilla.org"))
        sessionManager.add(Session("https://www.firefox.com").apply {
            customTabConfig = mock()
        })

        assertEquals(2, sessionManager.all.size)
        assertEquals(1, store.state.tabs.size)
        assertEquals(1, store.state.customTabs.size)

        sessionManager.removeAll()

        assertTrue(sessionManager.sessions.isEmpty())
        assertTrue(store.state.tabs.isEmpty())
        assertTrue(store.state.customTabs.isEmpty())
    }

    @Test
    fun `Selecting session`() {
        val store = BrowserStore()

        val sessionManager = SessionManager(engine = mock(), store = store)

        sessionManager.add(Session("https://www.mozilla.org"))
        sessionManager.add(Session("https://www.firefox.com"))

        sessionManager.select(sessionManager.sessions[1])

        val selectedTab = store.state.selectedTab
        assertNotNull(selectedTab!!)

        assertEquals("https://www.firefox.com", sessionManager.selectedSessionOrThrow.url)
        assertEquals("https://www.firefox.com", selectedTab.content.url)
    }

    @Test
    fun `Remove session and update selection`() {
        val store = BrowserStore()

        val manager = SessionManager(engine = mock(), store = store)

        val session1 = Session(id = "tab1", initialUrl = "https://www.mozilla.org")
        val session2 = Session(id = "tab2", initialUrl = "https://www.firefox.com")
        val session3 = Session(id = "tab3", initialUrl = "https://wiki.mozilla.org")
        val session4 = Session(id = "tab4", initialUrl = "https://github.com/mozilla-mobile/android-components")

        manager.add(session1)
        manager.add(session2)
        manager.add(session3)
        manager.add(session4)

        // (1), 2, 3, 4
        assertEquals(session1, manager.selectedSession)
        assertEquals("tab1", store.state.selectedTabId)

        // 1, 2, 3, (4)
        manager.select(session4)
        assertEquals(session4, manager.selectedSession)
        assertEquals("tab4", store.state.selectedTabId)

        // 1, 2, (3)
        manager.remove(session4)
        assertEquals(session3, manager.selectedSession)
        assertEquals("tab3", store.state.selectedTabId)

        // 2, (3)
        manager.remove(session1)
        assertEquals(session3, manager.selectedSession)
        assertEquals("tab3", store.state.selectedTabId)

        // (2), 3
        manager.select(session2)
        assertEquals(session2, manager.selectedSession)
        assertEquals("tab2", store.state.selectedTabId)

        // (2)
        manager.remove(session3)
        assertEquals(session2, manager.selectedSession)
        assertEquals("tab2", store.state.selectedTabId)

        // -
        manager.remove(session2)
        assertEquals(0, manager.size)
        assertNull(store.state.selectedTabId)
    }

    @Test
    fun `Remove private session and select nearby session`() {
        val store = BrowserStore()

        val manager = SessionManager(engine = mock(), store = store)
        assertNull(manager.selectedSession)

        val private1 = Session(id = "private1", initialUrl = "https://example.org/private1", private = true)
        manager.add(private1)

        val regular1 = Session(id = "regular1", initialUrl = "https://www.mozilla.org", private = false)
        manager.add(regular1)

        val regular2 = Session(id = "regular2", initialUrl = "https://www.firefox.com", private = false)
        manager.add(regular2)

        val private2 = Session(id = "private2", initialUrl = "https://example.org/private2", private = true)
        manager.add(private2)

        val private3 = Session(id = "private3", initialUrl = "https://example.org/private3", private = true)
        manager.add(private3)

        manager.select(private2)
        manager.remove(private2)
        assertEquals(private3, manager.selectedSession)
        assertEquals("private3", store.state.selectedTabId)

        manager.remove(private3)
        assertEquals(private1, manager.selectedSession)
        assertEquals("private1", store.state.selectedTabId)

        // Removing the last private session should cause a regular session to be selected
        manager.remove(private1)
        assertEquals(regular2, manager.selectedSession)
        assertEquals("regular2", store.state.selectedTabId)
    }

    @Test
    fun `Remove normal session and select nearby session`() {
        val store = BrowserStore()

        val manager = SessionManager(engine = mock(), store = store)
        assertNull(manager.selectedSession)
        assertNull(store.state.selectedTabId)

        val regular1 = Session(id = "regular1", initialUrl = "https://www.mozilla.org", private = false)
        manager.add(regular1)

        val private1 = Session(id = "private1", initialUrl = "https://example.org/private1", private = true)
        manager.add(private1)

        val private2 = Session(id = "private2", initialUrl = "https://example.org/private2", private = true)
        manager.add(private2)

        val regular2 = Session(id = "regular2", initialUrl = "https://www.firefox.com", private = false)
        manager.add(regular2)

        val regular3 = Session(id = "regular3", initialUrl = "https://www.firefox.org", private = false)
        manager.add(regular3)

        manager.select(regular2)
        manager.remove(regular2)
        assertEquals(regular3, manager.selectedSession)
        assertEquals("regular3", store.state.selectedTabId)

        manager.remove(regular3)
        assertEquals(regular1, manager.selectedSession)
        assertEquals("regular1", store.state.selectedTabId)

        // Removing the last regular session should NOT cause a private session to be selected
        manager.remove(regular1)
        assertNull(manager.selectedSession)
        assertNull(store.state.selectedTabId)
    }

    @Test
    fun `Restoring snapshot with invalid index`() {
        val store = BrowserStore()

        val manager = SessionManager(engine = mock(), store = store)
        manager.restore(SessionManager.Snapshot(listOf(), selectedSessionIndex = 0))

        assertEquals(0, manager.sessions.size)
        assertEquals(0, store.state.tabs.size)

        assertNull(manager.selectedSession)
        assertNull(store.state.selectedTabId)
    }

    @Test
    fun `Restoring snapshot without updating selection`() {
        val session: Session

        val store = BrowserStore()

        val manager = SessionManager(engine = mock(), store = store).apply {
            session = Session("https://getpocket.com")

            add(Session("https://www.mozilla.org"))
            add(session)
            add(Session("https://www.firefox.com"))
        }

        val item = manager.createSessionSnapshot(session)

        manager.remove(session)

        manager.restore(SessionManager.Snapshot.singleItem(item), updateSelection = false)

        assertEquals(3, manager.size)
        assertEquals(3, store.state.tabs.size)

        assertEquals("https://www.mozilla.org", manager.selectedSessionOrThrow.url)
        assertEquals("https://www.mozilla.org", store.state.selectedTab!!.content.url)

        assertEquals("https://getpocket.com", manager.sessions[0].url)
        assertEquals("https://getpocket.com", store.state.tabs[0].content.url)

        assertEquals("https://www.mozilla.org", manager.sessions[1].url)
        assertEquals("https://www.mozilla.org", store.state.tabs[1].content.url)

        assertEquals("https://www.firefox.com", manager.sessions[2].url)
        assertEquals("https://www.firefox.com", store.state.tabs[2].content.url)
    }

    @Test
    fun `Restoring snapshot into empty state`() {
        val snapshot = SessionManager.Snapshot(
            sessions = listOf(
                SessionManager.Snapshot.Item(
                    Session(id = "regular1", initialUrl = "https://www.mozilla.org", private = false)),
                SessionManager.Snapshot.Item(
                    Session(id = "private1", initialUrl = "https://example.org/private1", private = true)),
                SessionManager.Snapshot.Item(
                    Session(id = "private2", initialUrl = "https://example.org/private2", private = true)),
                SessionManager.Snapshot.Item(
                    Session(id = "regular2", initialUrl = "https://www.firefox.com", private = false)),
                SessionManager.Snapshot.Item(
                    Session(id = "regular3", initialUrl = "https://www.firefox.org", private = false))
            ),
            selectedSessionIndex = 2
        )

        val store = BrowserStore()
        val manager = SessionManager(engine = mock(), store = store)

        manager.restore(snapshot)

        assertEquals(5, manager.sessions.size)
        assertEquals(5, store.state.tabs.size)

        assertEquals("https://example.org/private2", manager.selectedSessionOrThrow.url)
        assertEquals("https://example.org/private2", store.state.selectedTab!!.content.url)
        assertEquals("private2", store.state.selectedTabId)

        manager.sessions.forEachIndexed { index, session ->
            assertEquals("Incorrect browser store on session ${session.id} at index $index", store, session.store)
        }
        assertEquals("https://www.mozilla.org", manager.sessions[0].url)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)

        assertEquals("https://example.org/private1", manager.sessions[1].url)
        assertEquals("https://example.org/private1", store.state.tabs[1].content.url)

        assertEquals("https://example.org/private2", manager.sessions[2].url)
        assertEquals("https://example.org/private2", store.state.tabs[2].content.url)

        assertEquals("https://www.firefox.com", manager.sessions[3].url)
        assertEquals("https://www.firefox.com", store.state.tabs[3].content.url)

        assertEquals("https://www.firefox.org", manager.sessions[4].url)
        assertEquals("https://www.firefox.org", store.state.tabs[4].content.url)
    }

    @Test
    fun `Remove session will not select custom tab`() {
        val session1 = Session(id = "session1", initialUrl = "https://www.firefox.com").apply {
            customTabConfig = mock() }
        val session2 = Session(id = "session2", initialUrl = "https://developer.mozilla.org/en-US/")
        val session3 = Session(id = "session3", initialUrl = "https://www.mozilla.org/en-US/internet-health/").apply {
            customTabConfig = mock() }
        val session4 = Session(id = "session4", initialUrl = "https://www.mozilla.org/en-US/technology/")
        val session5 = Session(id = "session5", initialUrl = "https://example.org/555").apply {
            customTabConfig = mock() }
        val session6 = Session(id = "session6", initialUrl = "https://example.org/Hello").apply {
            customTabConfig = mock() }
        val session7 = Session(id = "session7", initialUrl = "https://example.org/World").apply {
            customTabConfig = mock() }
        val session8 = Session(id = "session8", initialUrl = "https://example.org/JustTestingThings").apply {
            customTabConfig = mock() }
        val session9 = Session(id = "session9", initialUrl = "https://example.org/NoCustomTab")

        val store = BrowserStore()

        val manager = SessionManager(engine = mock(), store = store)
        manager.add(session1)
        manager.add(session2)
        manager.add(session3)
        manager.add(session4)
        manager.add(session5)
        manager.add(session6)
        manager.add(session7)
        manager.add(session8)
        manager.add(session9)

        manager.select(session4)
        assertEquals(session4, manager.selectedSession)
        assertEquals("session4", store.state.selectedTabId)

        manager.remove(session4)
        assertEquals(session9, manager.selectedSession)
        assertEquals("session9", store.state.selectedTabId)

        manager.remove(session9)
        assertEquals(session2, manager.selectedSession)
        assertEquals("session2", store.state.selectedTabId)

        manager.remove(session2)
        assertNull(manager.selectedSession)
        assertNull(store.state.selectedTabId)
    }

    /**
     * This test case:
     *
     * - Runs 10000 times
     * - Generates a random permutation of tabs and custom tabs
     * - Selects a random tab
     * - Randomly removes tabs
     * - Verifies that the selected tabs in BrowserStore and SessionManager match
     *
     * The `println` statements have been commented out to not spam the CI log. However they are helpful in case someone
     * needs to actually debug a problem here.
     */
    @Test
    fun `Remove session from randomized set will keep store and manager in sync`() {
        repeat(10000) {
            val numberOfSessions = (3..20).random()

            // println("Round $round ($numberOfSessions sessions)")

            val store = BrowserStore()
            val manager = SessionManager(engine = mock(), store = store)

            repeat(numberOfSessions) { number ->
                val isCustomTab = listOf(true, false).random()

                val session = Session("https://example.org/$number").apply {
                    if (isCustomTab) {
                        customTabConfig = mock()
                    }
                }

                manager.add(session)

                // println("- Session $number (ct = $isCustomTab, id = ${session.id})" )
            }

            if (manager.sessions.isNotEmpty()) {
                val select = manager.sessions.random()
                // println("Select initial session: $select")
                manager.select(select)
            }

            assertEquals(store.state.tabs.size, manager.sessions.size)
            assertEquals(store.state.tabs.size + store.state.customTabs.size, manager.all.size)

            assertEquals(store.state.selectedTabId, manager.selectedSession?.id)

            (3..3).random()

            repeat((3..numberOfSessions).random()) {
                val sessionToRemove = manager.all.random()

                // println("X Removing session ${sessionToRemove.id} (ct = ${sessionToRemove.isCustomTabSession()})")

                manager.remove(sessionToRemove)

                assertEquals(store.state.selectedTabId, manager.selectedSession?.id)
            }
        }
    }

    @Test
    fun `thumbnails of all but selected session should be removed on low memory`() {
        val store = BrowserStore()
        val sessionManager = SessionManager(engine = mock(), store = store)

        val session1 = Session("https://www.mozilla.org")
        val session2 = Session("https://getpocket.com")
        val session3 = Session("https://www.firefox.com")

        sessionManager.add(session1, false)
        session1.thumbnail = mock()
        sessionManager.add(session2, false)
        session2.thumbnail = mock()
        sessionManager.add(session3, true)
        session3.thumbnail = mock()

        val allSessionsMustHaveAThumbnail = store.state.tabs.all { it.content.thumbnail != null }
        assertTrue(allSessionsMustHaveAThumbnail)

        sessionManager.onLowMemory()

        assertNull(store.state.tabs[0].content.thumbnail)
        assertNull(store.state.tabs[1].content.thumbnail)
        // Thumbnail of selected session should not have been removed
        assertNotNull(store.state.tabs[2].content.thumbnail)
    }

    @Test
    fun `Adding multiple sessions into empty manager and store`() {
        val store = BrowserStore()
        val manager = SessionManager(engine = mock(), store = store)

        val sessions = listOf(
            Session(id = "tab1", initialUrl = "https://www.mozilla.org", private = true),
            Session(id = "tab2", initialUrl = "https://www.firefox.com"),
            Session(id = "tab3", initialUrl = "https://wiki.mozilla.org"))

        assertEquals(0, manager.sessions.size)
        assertEquals(0, store.state.tabs.size)

        manager.add(sessions)

        assertEquals(3, manager.sessions.size)
        assertEquals(3, store.state.tabs.size)

        assertEquals("https://www.mozilla.org", manager.sessions[0].url)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)

        assertEquals("https://www.firefox.com", manager.sessions[1].url)
        assertEquals("https://www.firefox.com", store.state.tabs[1].content.url)

        assertEquals("https://wiki.mozilla.org", manager.sessions[2].url)
        assertEquals("https://wiki.mozilla.org", store.state.tabs[2].content.url)

        assertEquals("https://www.firefox.com", manager.selectedSessionOrThrow.url)
        assertEquals("https://www.firefox.com", store.state.selectedTab!!.content.url)
    }

    @Test
    fun `Adding multiple sessions into manager and store with existing tabs`() {
        val store = BrowserStore()
        val manager = SessionManager(engine = mock(), store = store)

        manager.add(Session(id = "init0", initialUrl = "https://getpocket.com"))
        manager.add(Session(id = "init1", initialUrl = "https://www.google.com", private = true))

        val sessions = listOf(
                Session(id = "tab1", initialUrl = "https://www.mozilla.org", private = true),
                Session(id = "tab2", initialUrl = "https://www.firefox.com"),
                Session(id = "tab3", initialUrl = "https://wiki.mozilla.org"))

        assertEquals(2, manager.sessions.size)
        assertEquals(2, store.state.tabs.size)

        assertEquals("https://getpocket.com", manager.selectedSessionOrThrow.url)
        assertEquals("https://getpocket.com", store.state.selectedTab!!.content.url)

        assertEquals(2, manager.sessions.size)
        assertEquals(2, store.state.tabs.size)

        manager.add(sessions)

        assertEquals(5, manager.sessions.size)
        assertEquals(5, store.state.tabs.size)

        assertEquals("https://getpocket.com", manager.sessions[0].url)
        assertEquals("https://getpocket.com", store.state.tabs[0].content.url)

        assertEquals("https://www.google.com", manager.sessions[1].url)
        assertEquals("https://www.google.com", store.state.tabs[1].content.url)

        assertEquals("https://www.mozilla.org", manager.sessions[2].url)
        assertEquals("https://www.mozilla.org", store.state.tabs[2].content.url)

        assertEquals("https://www.firefox.com", manager.sessions[3].url)
        assertEquals("https://www.firefox.com", store.state.tabs[3].content.url)

        assertEquals("https://wiki.mozilla.org", manager.sessions[4].url)
        assertEquals("https://wiki.mozilla.org", store.state.tabs[4].content.url)

        assertEquals("https://getpocket.com", manager.selectedSessionOrThrow.url)
        assertEquals("https://getpocket.com", store.state.selectedTab!!.content.url)
    }

    @Test
    fun `Adding sessions with tracking protection state`() {
        val store = BrowserStore()
        val manager = SessionManager(engine = mock(), store = store)

        val tracker1: Tracker = mock()
        val tracker2: Tracker = mock()
        val tracker3: Tracker = mock()

        manager.add(Session(id = "session1", initialUrl = "https://www.mozilla.org").apply {
            trackerBlockingEnabled = true
            trackersBlocked = listOf(tracker1)
            trackersLoaded = listOf(tracker2, tracker3)
        })

        manager.add(Session(id = "session2", initialUrl = "https://www.mozilla.org").apply {
            trackerBlockingEnabled = false
            trackersBlocked = listOf()
            trackersLoaded = listOf(tracker2)
        })

        assertEquals(2, manager.sessions.size)
        assertEquals(2, store.state.tabs.size)

        store.state.findTab("session1")!!.also { tab ->
            assertTrue(tab.trackingProtection.enabled)
            assertEquals(1, tab.trackingProtection.blockedTrackers.size)
            assertEquals(2, tab.trackingProtection.loadedTrackers.size)
            assertTrue(tab.trackingProtection.blockedTrackers.contains(tracker1))
            assertTrue(tab.trackingProtection.loadedTrackers.contains(tracker2))
            assertTrue(tab.trackingProtection.loadedTrackers.contains(tracker3))
        }

        store.state.findTab("session2")!!.also { tab ->
            assertFalse(tab.trackingProtection.enabled)
            assertTrue(tab.trackingProtection.blockedTrackers.isEmpty())
            assertEquals(1, tab.trackingProtection.loadedTrackers.size)
            assertTrue(tab.trackingProtection.loadedTrackers.contains(tracker2))
        }
    }

    @Test
    fun `Updating tracking protection state of session`() {
        val store = BrowserStore()
        val manager = SessionManager(engine = mock(), store = store)

        val session = Session(id = "session", initialUrl = "https://www.mozilla.org")
        manager.add(session)

        store.state.findTab("session")!!.also { tab ->
            assertFalse(tab.trackingProtection.enabled)
            assertEquals(0, tab.trackingProtection.blockedTrackers.size)
            assertEquals(0, tab.trackingProtection.loadedTrackers.size)
        }

        session.trackerBlockingEnabled = true

        store.state.findTab("session")!!.also { tab ->
            assertTrue(tab.trackingProtection.enabled)
            assertEquals(0, tab.trackingProtection.blockedTrackers.size)
            assertEquals(0, tab.trackingProtection.loadedTrackers.size)
        }

        session.trackersBlocked = listOf(mock())
        session.trackersLoaded = listOf(mock())
        session.trackersBlocked = listOf(mock(), mock())

        store.state.findTab("session")!!.also { tab ->
            assertTrue(tab.trackingProtection.enabled)
            assertEquals(2, tab.trackingProtection.blockedTrackers.size)
            assertEquals(1, tab.trackingProtection.loadedTrackers.size)
        }

        session.trackersBlocked = emptyList()
        session.trackersLoaded = emptyList()

        store.state.findTab("session")!!.also { tab ->
            assertTrue(tab.trackingProtection.enabled)
            assertEquals(0, tab.trackingProtection.blockedTrackers.size)
            assertEquals(0, tab.trackingProtection.loadedTrackers.size)
        }

        session.trackerBlockingEnabled = false

        store.state.findTab("session")!!.also { tab ->
            assertFalse(tab.trackingProtection.enabled)
            assertEquals(0, tab.trackingProtection.blockedTrackers.size)
            assertEquals(0, tab.trackingProtection.loadedTrackers.size)
        }
    }

    @Test
    fun `Adding a hit result`() {
        val store = BrowserStore()
        val manager = SessionManager(engine = mock(), store = store)

        val session = Session(id = "session", initialUrl = "https://www.mozilla.org")
        manager.add(session)

        assertNull(session.hitResult.peek())
        assertNull(store.state.findTab("session")!!.content.hitResult)

        val hitResult: HitResult = HitResult.UNKNOWN("test")
        session.hitResult = Consumable.from(hitResult)

        assertEquals(hitResult, session.hitResult.peek())
        store.state.findTab("session")!!.also { tab ->
            assertNotNull(tab.content.hitResult)
            assertSame(hitResult, tab.content.hitResult)
        }

        session.hitResult.consume { true }
        store.state.findTab("session")!!.also { tab ->
            assertNull(tab.content.hitResult)
        }
    }

    @Test
    fun `Linking session to engine session`() {
        val store = BrowserStore()
        val engine: Engine = mock()

        val engineSession1: EngineSession = mock()
        doReturn(engineSession1).`when`(engine).createSession(false)

        val sessionManager = SessionManager(engine, store)

        val session = Session(id = "session", initialUrl = "https://www.mozilla.org")
        sessionManager.add(session)

        assertNull(sessionManager.getEngineSession(session))
        assertEquals(engineSession1, sessionManager.getOrCreateEngineSession(session))
        store.state.findTab("session")!!.also { tab ->
            assertEquals(engineSession1, tab.engineState.engineSession)
        }

        // Force unlink and link again
        val engineSession2: EngineSession = mock()
        doReturn(engineSession2).`when`(engine).createSession(false)
        session.engineSessionHolder.engineSession = null
        assertEquals(engineSession2, sessionManager.getOrCreateEngineSession(session))
        store.state.findTab("session")!!.also { tab ->
            assertEquals(engineSession2, tab.engineState.engineSession)
        }
    }

    @Test
    fun `Restoring engine session with state`() {
        val engine: Engine = mock()
        val store = BrowserStore()
        val sessionManager = SessionManager(engine, store)

        val engineSession: EngineSession = mock()
        val engineSessionState: EngineSessionState = mock()

        val snapshot = SessionManager.Snapshot(
            listOf(
                SessionManager.Snapshot.Item(
                    session = Session(id = "session1", initialUrl = "http://www.firefox.com"),
                    engineSessionState = engineSessionState
                ),
                SessionManager.Snapshot.Item(
                    session = Session(id = "session2", initialUrl = "http://www.mozilla.org"),
                    engineSession = engineSession
                )
            ),
            selectedSessionIndex = 0
        )

        sessionManager.restore(snapshot)
        assertEquals(2, sessionManager.size)

        store.state.findTab("session1")!!.also { tab ->
            assertEquals(engineSessionState, tab.engineState.engineSessionState)
        }

        store.state.findTab("session2")!!.also { tab ->
            assertEquals(engineSession, tab.engineState.engineSession)
        }
    }

    @Test
    fun `Adding a prompt request`() {
        val store = BrowserStore()
        val manager = SessionManager(engine = mock(), store = store)

        val session = Session(id = "session", initialUrl = "https://www.mozilla.org")
        manager.add(session)

        assertNull(session.promptRequest.peek())
        assertNull(store.state.findTab("session")!!.content.promptRequest)

        val promptRequest: PromptRequest = mock()
        session.promptRequest = Consumable.from(promptRequest)

        assertEquals(promptRequest, session.promptRequest.peek())
        store.state.findTab("session")!!.also { tab ->
            assertNotNull(tab.content.promptRequest)
            assertSame(promptRequest, tab.content.promptRequest)
        }

        session.promptRequest.consume { true }
        store.state.findTab("session")!!.also { tab ->
            assertNull(tab.content.promptRequest)
        }
    }

    @Test
    fun `Adding a find result`() {
        val store = BrowserStore()
        val manager = SessionManager(engine = mock(), store = store)

        val session = Session(id = "session", initialUrl = "https://www.mozilla.org")
        manager.add(session)

        assertTrue(session.findResults.isEmpty())
        assertTrue(store.state.findTab("session")!!.content.findResults.isEmpty())

        val result = Session.FindResult(0, 0, false)
        session.findResults += result

        assertEquals(1, session.findResults.size)
        assertEquals(result, session.findResults.last())
        store.state.findTab("session")!!.also { tab ->
            assertEquals(1, tab.content.findResults.size)
            assertEquals(result.toFindResultState(), tab.content.findResults.last())
        }

        session.findResults = emptyList()
        assertTrue(session.findResults.isEmpty())
        assertTrue(store.state.findTab("session")!!.content.findResults.isEmpty())
    }
}
