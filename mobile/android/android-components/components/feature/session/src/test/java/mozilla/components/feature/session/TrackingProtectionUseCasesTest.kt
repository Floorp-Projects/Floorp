/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.content.blocking.TrackerLog
import mozilla.components.concept.engine.content.blocking.TrackingProtectionExceptionStorage
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class TrackingProtectionUseCasesTest {

    private val mockSessionManager = mock<SessionManager>()
    private val mockSession = mock<EngineSession>()
    private val mockEngine = mock<Engine>()
    private val mockStore = mock<TrackingProtectionExceptionStorage>()
    private val useCases = TrackingProtectionUseCases(mockSessionManager, mockEngine)

    @Before
    fun setup() {
        whenever(mockSessionManager.getEngineSession(any())).thenReturn(mockSession)
        whenever(mockEngine.trackingProtectionExceptionStore).thenReturn(mockStore)
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

        whenever(mockEngine.getTrackersLog(any(), any(), any())).then {
            onSuccess(emptyList())
        }

        useCases.fetchTrackingLogs(mock(), onSuccess = {
            trackersLog = it
            onSuccessCalled = true
        }, onError = {
            onErrorCalled = true
        })

        verify(mockSessionManager).getEngineSession(any())
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
        whenever(mockSessionManager.getEngineSession(any())).thenReturn(null)

        whenever(mockEngine.getTrackersLog(any(), any(), any())).then {
            onSuccess(emptyList())
        }

        useCases.fetchTrackingLogs(mock(),
            onSuccess = {
                trackersLog = it
                onSuccessCalled = true
            },
            onError = {
                onErrorCalled = true
            })

        verify(mockSessionManager).getEngineSession(any())
        assertNull(trackersLog)
        assertTrue(onErrorCalled)
        assertFalse(onSuccessCalled)
    }

    @Test
    fun `add exception`() {
        useCases.addException(mock())
        verify(mockSessionManager).getEngineSession(any())
        verify(mockStore).add(any())
    }

    @Test
    fun `add exception with a null engine session will not call the store`() {
        whenever(mockSessionManager.getEngineSession(any())).thenReturn(null)

        useCases.addException(mock())
        verify(mockStore, times(0)).add(any())
    }

    @Test
    fun `remove exception`() {
        useCases.removeException(mock())
        verify(mockSessionManager).getEngineSession(any())
        verify(mockStore).remove(any())
    }

    @Test
    fun `remove exception with a null engine session will not call the store`() {
        whenever(mockSessionManager.getEngineSession(any())).thenReturn(null)

        useCases.removeException(mock())
        verify(mockStore, times(0)).remove(any())
    }

    @Test
    fun `removeAll exceptions`() {
        useCases.removeAllExceptions()
        verify(mockStore).removeAll()
    }

    @Test
    fun `contains exception`() {
        useCases.containsException(mock()) {}
        verify(mockStore).contains(any(), any())
    }

    @Test
    fun `contains exception with a null engine session will not call the store`() {
        var contains = true
        whenever(mockSessionManager.getEngineSession(any())).thenReturn(null)

        useCases.containsException(mock()) {
            contains = it
        }

        assertFalse(contains)
        verify(mockStore, times(0)).contains(any(), any())
    }

    @Test
    fun `fetch exceptions`() {
        useCases.fetchExceptions {}
        verify(mockStore).fetchAll(any())
    }
}
