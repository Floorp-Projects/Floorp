/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.TrackingProtectionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.content.blocking.TrackerLog
import mozilla.components.concept.engine.content.blocking.TrackingProtectionException
import mozilla.components.concept.engine.content.blocking.TrackingProtectionExceptionStorage
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class TrackingProtectionUseCasesTest {
    private lateinit var exceptionStore: TrackingProtectionExceptionStorage
    private lateinit var engine: Engine
    private lateinit var store: BrowserStore
    private lateinit var engineSession: EngineSession
    private lateinit var useCases: TrackingProtectionUseCases

    @Before
    fun setUp() {
        exceptionStore = mock()

        engine = mock()
        whenever(engine.trackingProtectionExceptionStore).thenReturn(exceptionStore)

        engineSession = mock()

        store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    TabSessionState(
                        id = "A",
                        content = ContentState("https://www.mozilla.org"),
                        engineState = EngineState(engineSession),
                    ),
                ),
                selectedTabId = "A",
            ),
        )

        useCases = TrackingProtectionUseCases(store, engine)
    }

    @Test
    fun `fetch trackers logged successfully`() {
        var trackersLog: List<TrackerLog>? = null
        var onSuccessCalled = false
        var onErrorCalled = false
        val onSuccess: (List<TrackerLog>) -> Unit = {
            trackersLog = it
            onSuccessCalled = true
        }

        whenever(engine.getTrackersLog(any(), any(), any())).then {
            onSuccess(emptyList())
        }

        val useCases = TrackingProtectionUseCases(store, engine)

        useCases.fetchTrackingLogs(
            "A",
            onSuccess = {
                trackersLog = it
                onSuccessCalled = true
            },
            onError = {
                onErrorCalled = true
            },
        )

        assertNotNull(trackersLog)
        assertTrue(onSuccessCalled)
        assertFalse(onErrorCalled)
    }

    @Test
    fun `calling fetchTrackingLogs with a null engine session will call onError`() {
        var trackersLog: List<TrackerLog>? = null
        var onSuccessCalled = false
        var onErrorCalled = false
        val onSuccess: (List<TrackerLog>) -> Unit = {
            trackersLog = it
            onSuccessCalled = true
        }

        store.dispatch(
            EngineAction.UnlinkEngineSessionAction("A"),
        ).joinBlocking()

        whenever(engine.getTrackersLog(any(), any(), any())).then {
            onSuccess(emptyList())
        }

        useCases.fetchTrackingLogs(
            "A",
            onSuccess = {
                trackersLog = it
                onSuccessCalled = true
            },
            onError = {
                onErrorCalled = true
            },
        )

        assertNull(trackersLog)
        assertTrue(onErrorCalled)
        assertFalse(onSuccessCalled)
    }

    @Test
    fun `add exception`() {
        useCases.addException("A")

        verify(exceptionStore).add(engineSession)
    }

    @Test
    fun `add exception with a null engine session will not call the store`() {
        store.dispatch(
            EngineAction.UnlinkEngineSessionAction("A"),
        ).joinBlocking()

        useCases.addException("A")

        verify(exceptionStore, never()).add(any(), eq(false))
    }

    @Test
    fun `GIVEN a persistent in private mode exception WHEN adding THEN add the exception and indicate that it is persistent`() {
        val tabId = "A"
        useCases.addException(tabId, persistInPrivateMode = true)

        verify(exceptionStore).add(engineSession, true)
    }

    @Test
    fun `remove a session exception`() {
        useCases.removeException("A")

        verify(exceptionStore).remove(engineSession)
    }

    @Test
    fun `remove a tracking protection exception`() {
        val tab1 = createTab("https://www.mozilla.org")
            .copy(trackingProtection = TrackingProtectionState(ignoredOnTrackingProtection = true))

        val tab2 = createTab("https://wiki.mozilla.org/")
            .copy(trackingProtection = TrackingProtectionState(ignoredOnTrackingProtection = true))

        val tab3 = createTab("https://www.mozilla.org/en-CA/")
            .copy(trackingProtection = TrackingProtectionState(ignoredOnTrackingProtection = true))

        val customTab = createCustomTab("https://www.mozilla.org/en-CA/")
            .copy(trackingProtection = TrackingProtectionState(ignoredOnTrackingProtection = true))

        val exception = object : TrackingProtectionException {
            override val url: String = tab1.content.url
        }

        store.dispatch(TabListAction.AddTabAction(tab1)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab2)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab3)).joinBlocking()
        store.dispatch(CustomTabListAction.AddCustomTabAction(customTab)).joinBlocking()
        store.waitUntilIdle()

        assertTrue(store.state.findTab(tab1.id)!!.trackingProtection.ignoredOnTrackingProtection)
        assertTrue(store.state.findTab(tab2.id)!!.trackingProtection.ignoredOnTrackingProtection)
        assertTrue(store.state.findTab(tab3.id)!!.trackingProtection.ignoredOnTrackingProtection)
        assertTrue(store.state.findCustomTab(customTab.id)!!.trackingProtection.ignoredOnTrackingProtection)

        useCases.removeException(exception)

        verify(exceptionStore).remove(exception)

        store.waitUntilIdle()

        assertFalse(store.state.findTab(tab1.id)!!.trackingProtection.ignoredOnTrackingProtection)

        // Different domain from tab1 MUST not be affected
        assertTrue(store.state.findTab(tab2.id)!!.trackingProtection.ignoredOnTrackingProtection)

        // Another tabs with the same domain as tab1 MUST be updated
        assertFalse(store.state.findTab(tab3.id)!!.trackingProtection.ignoredOnTrackingProtection)
        assertFalse(store.state.findCustomTab(customTab.id)!!.trackingProtection.ignoredOnTrackingProtection)
    }

    @Test
    fun `remove exception with a null engine session will not call the store`() {
        store.dispatch(
            EngineAction.UnlinkEngineSessionAction("A"),
        ).joinBlocking()

        useCases.removeException("A")

        verify(exceptionStore, never()).remove(any<EngineSession>())
    }

    @Test
    fun `removeAll exceptions`() {
        useCases.removeAllExceptions()

        verify(exceptionStore).removeAll(any(), any())
    }

    @Test
    fun `contains exception`() {
        val callback: (Boolean) -> Unit = {}
        useCases.containsException("A", callback)

        verify(exceptionStore).contains(engineSession, callback)
    }

    @Test
    fun `contains exception with a null engine session will not call the store`() {
        var contains = true

        store.dispatch(
            EngineAction.UnlinkEngineSessionAction("A"),
        ).joinBlocking()

        useCases.containsException("A") {
            contains = it
        }

        assertFalse(contains)
        verify(exceptionStore, never()).contains(any(), any())
    }

    @Test
    fun `fetch exceptions`() {
        useCases.fetchExceptions {}
        verify(exceptionStore).fetchAll(any())
    }
}
