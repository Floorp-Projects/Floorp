/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.content.blocking.TrackerLog
import mozilla.components.concept.engine.content.blocking.TrackingProtectionException
import mozilla.components.concept.engine.content.blocking.TrackingProtectionExceptionStorage
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

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

        store = BrowserStore(BrowserState(
            tabs = listOf(
                TabSessionState(
                    id = "A",
                    content = ContentState("https://www.mozilla.org"),
                    engineState = EngineState(engineSession)
                )
            ),
            selectedTabId = "A"
        ))

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

        useCases.fetchTrackingLogs("A", onSuccess = {
            trackersLog = it
            onSuccessCalled = true
        }, onError = {
            onErrorCalled = true
        })

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
            EngineAction.UnlinkEngineSessionAction("A")
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
            })

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
            EngineAction.UnlinkEngineSessionAction("A")
        ).joinBlocking()

        useCases.addException("A")

        verify(exceptionStore, never()).add(any())
    }

    @Test
    fun `remove a session exception`() {
        useCases.removeException("A")

        verify(exceptionStore).remove(engineSession)
    }

    @Test
    fun `remove a tracking protection exception`() {
        val exception: TrackingProtectionException = mock()

        useCases.removeException(exception)
        verify(exceptionStore).remove(exception)
    }

    @Test
    fun `remove exception with a null engine session will not call the store`() {
        store.dispatch(
            EngineAction.UnlinkEngineSessionAction("A")
        ).joinBlocking()

        useCases.removeException("A")

        verify(exceptionStore, never()).remove(any<EngineSession>())
    }

    @Test
    fun `removeAll exceptions`() {
        useCases.removeAllExceptions()

        verify(exceptionStore).removeAll(any())
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
            EngineAction.UnlinkEngineSessionAction("A")
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
