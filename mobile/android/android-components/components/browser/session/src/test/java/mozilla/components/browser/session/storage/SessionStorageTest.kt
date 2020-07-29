/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.ext.toTabSessionState
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState.Source
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
class SessionStorageTest {

    @Test
    fun `Restored snapshot should contain sessions of saved snapshot`() {
        val session1 = Session("http://mozilla.org", id = "session1")
        val session2 = Session("http://getpocket.com", id = "session2", contextId = "2")
        val session3 = Session("http://getpocket.com", id = "session3")
        session3.parentId = "session1"

        val engineSessionState = object : EngineSessionState {
            override fun toJSON() = JSONObject()
        }

        val engineSession = mock(EngineSession::class.java)
        `when`(engineSession.saveState()).thenReturn(engineSessionState)

        val engine = mock(Engine::class.java)
        `when`(engine.name()).thenReturn("gecko")
        `when`(engine.createSession()).thenReturn(mock(EngineSession::class.java))
        `when`(engine.createSessionState(any())).thenReturn(engineSessionState)

        // Persist the state
        val state = BrowserState(tabs = listOf(
                session1.toTabSessionState(),
                session2.toTabSessionState(),
                session3.toTabSessionState()
            ),
            selectedTabId = session1.id
        )

        val storage = SessionStorage(testContext, engine)
        val persisted = storage.save(state)
        assertTrue(persisted)

        // Read it back
        val restoredSnapshot = storage.restore()
        assertNotNull(restoredSnapshot)
        assertEquals(3, restoredSnapshot!!.sessions.size)
        assertEquals(0, restoredSnapshot.selectedSessionIndex)

        assertEquals(session1, restoredSnapshot.sessions[0].session)
        assertEquals(session1.url, restoredSnapshot.sessions[0].session.url)
        assertEquals(session1.id, restoredSnapshot.sessions[0].session.id)
        assertNull(restoredSnapshot.sessions[0].session.parentId)
        assertNull(restoredSnapshot.sessions[0].session.contextId)

        assertEquals(session2, restoredSnapshot.sessions[1].session)
        assertEquals(session2.url, restoredSnapshot.sessions[1].session.url)
        assertEquals(session2.id, restoredSnapshot.sessions[1].session.id)
        assertNull(restoredSnapshot.sessions[1].session.parentId)
        assertEquals(session2.contextId, restoredSnapshot.sessions[1].session.contextId)

        assertEquals(session3, restoredSnapshot.sessions[2].session)
        assertEquals(session3.url, restoredSnapshot.sessions[2].session.url)
        assertEquals(session3.id, restoredSnapshot.sessions[2].session.id)
        assertEquals("session1", restoredSnapshot.sessions[2].session.parentId)
        assertNull(restoredSnapshot.sessions[2].session.contextId)

        val restoredEngineSession = restoredSnapshot.sessions[0].engineSessionState
        assertNotNull(restoredEngineSession)
    }

    @Test
    fun `Saving empty state`() {
        val engine = mock(Engine::class.java)
        `when`(engine.name()).thenReturn("gecko")

        val storage = spy(SessionStorage(testContext, engine))
        storage.save(BrowserState())

        verify(storage).clear()

        val snapshot = storage.restore()
        assertNull(snapshot)
    }

    @Test
    fun `Should return empty snapshot after clearing`() {
        val session1 = Session("http://mozilla.org", id = "session1")
        val session2 = Session("http://getpocket.com", id = "session2")

        val engine = mock(Engine::class.java)
        `when`(engine.name()).thenReturn("gecko")
        `when`(engine.createSession()).thenReturn(mock(EngineSession::class.java))

        // Persist the state
        val state = BrowserState(tabs = listOf(
                session1.toTabSessionState(),
                session2.toTabSessionState()
            ),
            selectedTabId = session1.id
        )

        val storage = SessionStorage(testContext, engine)
        val persisted = storage.save(state)
        assertTrue(persisted)

        storage.clear()

        // Read it back. Expect null, indicating empty.
        val restoredSnapshot = storage.restore()
        assertNull(restoredSnapshot)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `Should throw when saving illegal state`() {
        val engine = mock(Engine::class.java)
        `when`(engine.name()).thenReturn("gecko")

        val storage = SessionStorage(testContext, engine)
        val session = Session("http://mozilla.org")
        val state = BrowserState(selectedTabId = "invalid", tabs = listOf(session.toTabSessionState()))
        storage.save(state)
    }

    @Test
    fun `AutoSave - when going to background`() {
        runBlocking {
            // Keep the "owner" in scope to avoid it getting garbage collected and therefore lifecycle events
            // not getting propagated (See #1428).
            val owner = mock(LifecycleOwner::class.java)
            val lifecycle = LifecycleRegistry(owner)

            val sessionStorage: SessionStorage = mock()

            val state = BrowserState()
            val store = BrowserStore(state)
            val autoSave = AutoSave(
                store = store,
                sessionStorage = sessionStorage,
                minimumIntervalMs = 0
            ).whenGoingToBackground(lifecycle)

            verifyNoMoreInteractions(sessionStorage)

            lifecycle.handleLifecycleEvent(Lifecycle.Event.ON_CREATE)
            lifecycle.handleLifecycleEvent(Lifecycle.Event.ON_START)
            lifecycle.handleLifecycleEvent(Lifecycle.Event.ON_RESUME)
            lifecycle.handleLifecycleEvent(Lifecycle.Event.ON_PAUSE)

            verifyNoMoreInteractions(sessionStorage)

            lifecycle.handleLifecycleEvent(Lifecycle.Event.ON_STOP)

            autoSave.saveJob!!.join()

            verify(sessionStorage).save(state)
        }
    }

    @Test
    fun `AutoSave - when session gets added`() {
        runBlocking {
            val state = BrowserState()
            val store = BrowserStore(state)

            val sessionManager = SessionManager(mock(), store)
            val sessionStorage: SessionStorage = mock()

            val dispatcher = TestCoroutineDispatcher()
            val scope = CoroutineScope(dispatcher)

            val autoSave = AutoSave(
                store = store,
                sessionStorage = sessionStorage,
                minimumIntervalMs = 0
            ).whenSessionsChange(scope)

            dispatcher.advanceUntilIdle()

            assertNull(autoSave.saveJob)
            verify(sessionStorage, never()).save(any())

            sessionManager.add(Session("https://www.mozilla.org"))
            dispatcher.advanceUntilIdle()

            autoSave.saveJob?.join()

            verify(sessionStorage).save(any())
        }
    }

    @Test
    fun `AutoSave - when session gets removed`() {
        runBlocking {
            val sessionStorage: SessionStorage = mock()

            val state = BrowserState()
            val store = BrowserStore(state)

            val sessionManager = SessionManager(mock(), store)
            sessionManager.add(Session("https://www.firefox.com"))
            val session = Session("https://www.mozilla.org").also {
                sessionManager.add(it)
            }

            val dispatcher = TestCoroutineDispatcher()
            val scope = CoroutineScope(dispatcher)

            val autoSave = AutoSave(
                store = store,
                sessionStorage = sessionStorage,
                minimumIntervalMs = 0
            ).whenSessionsChange(scope)

            dispatcher.advanceUntilIdle()

            assertNull(autoSave.saveJob)
            verify(sessionStorage, never()).save(any())

            sessionManager.remove(session)
            dispatcher.advanceUntilIdle()

            autoSave.saveJob?.join()

            verify(sessionStorage).save(any())
        }
    }

    @Test
    fun `AutoSave - when all sessions get removed`() {
        runBlocking {
            val state = BrowserState()
            val store = BrowserStore(state)

            val sessionManager = SessionManager(mock(), store)
            sessionManager.add(Session("https://www.firefox.com"))
            sessionManager.add(Session("https://www.mozilla.org"))

            val sessionStorage: SessionStorage = mock()

            val dispatcher = TestCoroutineDispatcher()
            val scope = CoroutineScope(dispatcher)

            val autoSave = AutoSave(
                store = store,
                sessionStorage = sessionStorage,
                minimumIntervalMs = 0
            ).whenSessionsChange(scope)

            dispatcher.advanceUntilIdle()

            assertNull(autoSave.saveJob)
            verify(sessionStorage, never()).save(any())

            sessionManager.removeAll()
            dispatcher.advanceUntilIdle()

            autoSave.saveJob?.join()

            verify(sessionStorage).save(any())
        }
    }

    @Test
    fun `AutoSave - when no sessions left`() {
        runBlocking {
            val state = BrowserState()
            val store = BrowserStore(state)

            val session = Session("https://www.firefox.com")
            val sessionManager = SessionManager(mock(), store)
            sessionManager.add(session)

            val sessionStorage: SessionStorage = mock()

            val dispatcher = TestCoroutineDispatcher()
            val scope = CoroutineScope(dispatcher)

            val autoSave = AutoSave(
                store = store,
                sessionStorage = sessionStorage,
                minimumIntervalMs = 0
            ).whenSessionsChange(scope)

            dispatcher.advanceUntilIdle()

            assertNull(autoSave.saveJob)
            verify(sessionStorage, never()).save(any())

            // We didn't specify a default session lambda so this will
            // leave us without a session
            sessionManager.remove(session)
            dispatcher.advanceUntilIdle()
            assertEquals(0, sessionManager.size)

            autoSave.saveJob?.join()

            verify(sessionStorage).save(any())
        }
    }

    @Test
    fun `AutoSave - when session gets selected`() {
        runBlocking {
            val state = BrowserState()
            val store = BrowserStore(state)

            val sessionManager = SessionManager(mock(), store)
            sessionManager.add(Session("https://www.firefox.com"))
            val session = Session("https://www.mozilla.org").also {
                sessionManager.add(it)
            }

            val sessionStorage: SessionStorage = mock()

            val dispatcher = TestCoroutineDispatcher()
            val scope = CoroutineScope(dispatcher)

            val autoSave = AutoSave(
                store = store,
                sessionStorage = sessionStorage,
                minimumIntervalMs = 0
            ).whenSessionsChange(scope)

            dispatcher.advanceUntilIdle()

            assertNull(autoSave.saveJob)
            verify(sessionStorage, never()).save(any())

            sessionManager.select(session)
            dispatcher.advanceUntilIdle()

            autoSave.saveJob?.join()

            verify(sessionStorage).save(any())
        }
    }

    @Test
    fun `AutoSave - when session loading state changes`() {
        runBlocking {
            val sessionStorage: SessionStorage = mock()

            val state = BrowserState()
            val store = BrowserStore(state)

            val sessionManager = SessionManager(mock(), store)
            val session = Session("https://www.mozilla.org").also {
                sessionManager.add(it)
            }

            val dispatcher = TestCoroutineDispatcher()
            val scope = CoroutineScope(dispatcher)

            val autoSave = AutoSave(
                store = store,
                sessionStorage = sessionStorage,
                minimumIntervalMs = 0
            ).whenSessionsChange(scope)

            session.loading = true
            dispatcher.advanceUntilIdle()

            assertNull(autoSave.saveJob)
            verify(sessionStorage, never()).save(any())

            session.loading = false
            dispatcher.advanceUntilIdle()

            autoSave.saveJob?.join()

            verify(sessionStorage).save(any())
        }
    }

    @Test
    fun `AutoSave - periodically in foreground`() {
        val engine = mock(Engine::class.java)
        val scheduler = mock(ScheduledExecutorService::class.java)
        val scheduledFuture = mock(ScheduledFuture::class.java)
        `when`(scheduler.scheduleAtFixedRate(any(),
                ArgumentMatchers.eq(300L),
                ArgumentMatchers.eq(300L),
                ArgumentMatchers.eq(TimeUnit.SECONDS))).thenReturn(scheduledFuture)

        // LifecycleRegistry only keeps a weak reference to the owner, so it is important to keep
        // a reference here too during the test run.
        // See https://github.com/mozilla-mobile/android-components/issues/5166
        val owner = mock(LifecycleOwner::class.java)
        val lifecycle = LifecycleRegistry(owner)

        val state = BrowserState()
        val store = BrowserStore(state)
        val storage = SessionStorage(testContext, engine)
        storage.autoSave(store)
            .periodicallyInForeground(300, TimeUnit.SECONDS, scheduler, lifecycle)

        verifyNoMoreInteractions(scheduler)

        lifecycle.handleLifecycleEvent(Lifecycle.Event.ON_START)

        verify(scheduler).scheduleAtFixedRate(any(),
                ArgumentMatchers.eq(300L),
                ArgumentMatchers.eq(300L),
                ArgumentMatchers.eq(TimeUnit.SECONDS))

        verifyNoMoreInteractions(scheduler)

        lifecycle.handleLifecycleEvent(Lifecycle.Event.ON_STOP)

        verify(scheduledFuture).cancel(false)
    }

    @Test
    fun `AutoSave - No new job triggered while save in flight`() {
        val sessionStorage: SessionStorage = mock()

        val state = BrowserState()
        val store = BrowserStore(state)
        val autoSave = AutoSave(
            store = store,
            sessionStorage = sessionStorage,
            minimumIntervalMs = 0
        )

        val runningJob: Job = mock()
        doReturn(true).`when`(runningJob).isActive

        val saveJob = autoSave.triggerSave()
        assertSame(saveJob, saveJob)
    }

    @Test
    fun `AutoSave - New job triggered if current job is done`() {
        val sessionStorage: SessionStorage = mock()

        val state = BrowserState()
        val store = BrowserStore(state)
        val autoSave = AutoSave(
            store = store,
            sessionStorage = sessionStorage,
            minimumIntervalMs = 0
        )

        val completed: Job = mock()
        doReturn(false).`when`(completed).isActive

        val saveJob = autoSave.triggerSave()
        assertNotSame(completed, saveJob)
    }

    @Test
    fun deserializeWithInvalidSource() {
        val json = JSONObject()
        json.put("uuid", "testId")
        json.put("url", "testUrl")
        json.put("source", Source.NEW_TAB.name)
        json.put("parentUuid", "")
        assertEquals(
            Source.RESTORED,
            deserializeSession(json, restoreId = true, restoreParentId = true).source
        )

        val jsonInvalid = JSONObject()
        jsonInvalid.put("uuid", "testId")
        jsonInvalid.put("url", "testUrl")
        jsonInvalid.put("source", "invalidSource")
        jsonInvalid.put("parentUuid", "")
        assertEquals(
            Source.RESTORED,
            deserializeSession(jsonInvalid, restoreId = true, restoreParentId = true).source
        )
    }

    @Suppress("UNCHECKED_CAST")
    private fun <T> anyJson(): T {
        any<T>()
        return null as T
    }
}