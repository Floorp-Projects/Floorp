/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.app.Activity
import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import mozilla.components.browser.engine.gecko.content.blocking.GeckoTrackingProtectionException
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.content.blocking.TrackingProtectionException
import mozilla.components.support.ktx.util.readAndDeserialize
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mozilla.geckoview.ContentBlockingController
import org.mozilla.geckoview.ContentBlockingController.ContentBlockingException
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession
import org.robolectric.Robolectric.buildActivity

@RunWith(AndroidJUnit4::class)
class TrackingProtectionExceptionFileStorageTest {

    private lateinit var runtime: GeckoRuntime

    private val context: Context
        get() = buildActivity(Activity::class.java).get()

    @Before
    fun setup() {
        runtime = mock()
        whenever(runtime.settings).thenReturn(mock())
    }

    @Test
    fun `restoreAsync exception`() {
        val mockContentBlocking = mock<ContentBlockingController>()
        val session = mock<GeckoEngineSession>()
        val geckoResult = GeckoResult<List<ContentBlockingException>>()
        val mockGeckoSession = mock<GeckoSession>()
        val mockExceptionList =
            listOf(ContentBlockingException.fromJson(JSONObject("{\"principal\":\"eyIxIjp7IjAiOiJodHRwczovL3d3dy5jbm4uY29tLyJ9fQ==\",\"uri\":\"https:\\/\\/www.cnn.com\\/\"}")))

        whenever(session.geckoSession).thenReturn(mockGeckoSession)
        whenever(runtime.contentBlockingController).thenReturn(mockContentBlocking)
        whenever(runtime.contentBlockingController.saveExceptionList()).thenReturn(geckoResult)

        val storage = TrackingProtectionExceptionFileStorage(testContext, runtime)
        storage.scope = CoroutineScope(Dispatchers.Main)

        assertNull(storage.getFile(context).readAndDeserialize { })

        storage.add(session)
        geckoResult.complete(mockExceptionList)

        storage.restore()

        verify(mockContentBlocking).restoreExceptionList(any<List<ContentBlockingException>>())
        assertNotNull(storage.getFile(context).readAndDeserialize { })
    }

    @Test
    fun `add exception`() {
        val mockContentBlocking = mock<ContentBlockingController>()
        val session = spy(GeckoEngineSession(runtime))
        val geckoResult = GeckoResult<List<ContentBlockingException>>()
        val mockGeckoSession = mock<GeckoSession>()
        val mockExceptionList =
            listOf(ContentBlockingException.fromJson(JSONObject("{\"principal\":\"eyIxIjp7IjAiOiJodHRwczovL3d3dy5jbm4uY29tLyJ9fQ==\",\"uri\":\"https:\\/\\/www.cnn.com\\/\"}")))
        var excludedOnTrackingProtection = false

        whenever(session.geckoSession).thenReturn(mockGeckoSession)
        whenever(runtime.contentBlockingController).thenReturn(mockContentBlocking)
        whenever(runtime.contentBlockingController.saveExceptionList()).thenReturn(geckoResult)

        val storage = TrackingProtectionExceptionFileStorage(testContext, runtime)
        storage.scope = CoroutineScope(Dispatchers.Main)

        assertNull(storage.getFile(context).readAndDeserialize { })
        session.register(object : EngineSession.Observer {
            override fun onExcludedOnTrackingProtectionChange(excluded: Boolean) {
                excludedOnTrackingProtection = excluded
            }
        })

        storage.add(session)
        geckoResult.complete(mockExceptionList)

        verify(mockContentBlocking).addException(mockGeckoSession)
        verify(mockContentBlocking).saveExceptionList()
        assertTrue(excludedOnTrackingProtection)
        assertNotNull(storage.getFile(context).readAndDeserialize { })
    }

    @Test
    fun `remove all exceptions`() {
        val mockContentBlocking = mock<ContentBlockingController>()
        val session = mock<GeckoEngineSession>()
        val geckoResult = GeckoResult<List<ContentBlockingException>>()
        val mockGeckoSession = mock<GeckoSession>()
        val mockExceptionList =
            listOf(ContentBlockingException.fromJson(JSONObject("{\"principal\":\"eyIxIjp7IjAiOiJodHRwczovL3d3dy5jbm4uY29tLyJ9fQ==\",\"uri\":\"https:\\/\\/www.cnn.com\\/\"}")))
        val engineSession: EngineSession = mock()
        val activeSessions: List<EngineSession> = listOf(engineSession)

        whenever(session.geckoSession).thenReturn(mockGeckoSession)
        whenever(runtime.contentBlockingController).thenReturn(mockContentBlocking)
        whenever(runtime.contentBlockingController.saveExceptionList()).thenReturn(geckoResult)

        // Adding exception
        val storage = TrackingProtectionExceptionFileStorage(testContext, runtime)
        storage.scope = CoroutineScope(Dispatchers.Main)

        assertNull(storage.getFile(context).readAndDeserialize { })

        storage.add(session)
        geckoResult.complete(mockExceptionList)

        verify(mockContentBlocking).addException(mockGeckoSession)
        verify(mockContentBlocking).saveExceptionList()
        assertNotNull(storage.getFile(context).readAndDeserialize { })

        // Removing exceptions
        storage.removeAll(activeSessions)
        verify(mockContentBlocking).clearExceptionList()
        assertNull(storage.getFile(context).readAndDeserialize { })
        verify(engineSession).notifyObservers(any())
    }

    @Test
    fun `remove exception`() {
        val mockContentBlocking = mock<ContentBlockingController>()
        val session = spy(GeckoEngineSession(runtime))
        var geckoResult = GeckoResult<List<ContentBlockingException>>()
        val mockGeckoSession = mock<GeckoSession>()
        val mockExceptionList =
            listOf(ContentBlockingException.fromJson(JSONObject("{\"principal\":\"eyIxIjp7IjAiOiJodHRwczovL3d3dy5jbm4uY29tLyJ9fQ==\",\"uri\":\"https:\\/\\/www.cnn.com\\/\"}")))
        var excludedOnTrackingProtection = false

        whenever(session.geckoSession).thenReturn(mockGeckoSession)
        whenever(runtime.contentBlockingController).thenReturn(mockContentBlocking)
        whenever(runtime.contentBlockingController.saveExceptionList()).thenReturn(geckoResult)

        // Adding exception
        val storage = TrackingProtectionExceptionFileStorage(testContext, runtime)
        storage.scope = CoroutineScope(Dispatchers.Main)

        assertNull(storage.getFile(context).readAndDeserialize { })
        session.register(object : EngineSession.Observer {
            override fun onExcludedOnTrackingProtectionChange(excluded: Boolean) {
                excludedOnTrackingProtection = excluded
            }
        })

        storage.add(session)
        geckoResult.complete(mockExceptionList)

        verify(mockContentBlocking).addException(mockGeckoSession)
        verify(mockContentBlocking).saveExceptionList()
        assertNotNull(storage.getFile(context).readAndDeserialize { })
        assertTrue(excludedOnTrackingProtection)

        // Removing exception
        geckoResult = GeckoResult<List<ContentBlockingException>>()
        whenever(runtime.contentBlockingController.saveExceptionList()).thenReturn(geckoResult)
        storage.remove(session)
        verify(mockContentBlocking).removeException(mockGeckoSession)
        geckoResult.complete(null)
        assertNull(storage.getFile(context).readAndDeserialize { })
        assertFalse(excludedOnTrackingProtection)
    }

    @Test
    fun `remove a TrackingProtectionException`() {
        val mockContentBlocking = mock<ContentBlockingController>()
        val session = spy(GeckoEngineSession(runtime))
        var geckoResult = GeckoResult<List<ContentBlockingException>>()
        val mockGeckoSession = mock<GeckoSession>()
        val mockExceptionList =
            listOf(ContentBlockingException.fromJson(JSONObject("{\"principal\":\"eyIxIjp7IjAiOiJodHRwczovL3d3dy5jbm4uY29tLyJ9fQ==\",\"uri\":\"https:\\/\\/www.cnn.com\\/\"}")))

        whenever(session.geckoSession).thenReturn(mockGeckoSession)
        whenever(runtime.contentBlockingController).thenReturn(mockContentBlocking)
        whenever(runtime.contentBlockingController.saveExceptionList()).thenReturn(geckoResult)

        // Adding exception
        val storage = TrackingProtectionExceptionFileStorage(testContext, runtime)
        storage.scope = CoroutineScope(Dispatchers.Main)

        storage.add(session)
        geckoResult.complete(mockExceptionList)
        assertNotNull(storage.getFile(context).readAndDeserialize { })

        // Removing exception
        geckoResult = GeckoResult()
        whenever(runtime.contentBlockingController.saveExceptionList()).thenReturn(geckoResult)
        storage.remove(
            GeckoTrackingProtectionException(
                "https://www.cnn.com/",
                "eyIxIjp7IjAiOiJodHRwczovL3d3dy5jbm4uY29tLyJ9fQ"
            )
        )
        verify(mockContentBlocking).removeException(any<ContentBlockingException>())
        geckoResult.complete(null)
        assertNull(storage.getFile(context).readAndDeserialize { })
    }

    @Test
    fun `contains exception`() {
        val mockContentBlocking = mock<ContentBlockingController>()
        val session = mock<GeckoEngineSession>()
        var geckoResult = GeckoResult<Boolean>()
        val mockGeckoSession = mock<GeckoSession>()
        var containsException = false
        whenever(session.geckoSession).thenReturn(mockGeckoSession)
        whenever(runtime.contentBlockingController).thenReturn(mockContentBlocking)
        whenever(runtime.contentBlockingController.checkException(mockGeckoSession)).thenReturn(geckoResult)

        val storage = TrackingProtectionExceptionFileStorage(testContext, runtime)

        storage.contains(session) { contains ->
            containsException = contains
        }
        geckoResult.complete(true)

        verify(mockContentBlocking).checkException(mockGeckoSession)
        assertTrue(containsException)

        geckoResult = GeckoResult()
        whenever(runtime.contentBlockingController.checkException(mockGeckoSession)).thenReturn(geckoResult)

        storage.contains(session) { contains ->
            containsException = contains
        }
        geckoResult.complete(null)
        assertFalse(containsException)
    }

    @Test
    fun `getAll exceptions`() {
        val mockContentBlocking = mock<ContentBlockingController>()
        val session = mock<GeckoEngineSession>()
        var geckoResult = GeckoResult<List<ContentBlockingException>>()
        val mockGeckoSession = mock<GeckoSession>()
        val mockExceptionList =
            listOf(ContentBlockingException.fromJson(JSONObject("{\"principal\":\"eyIxIjp7IjAiOiJodHRwczovL3d3dy5jbm4uY29tLyJ9fQ==\",\"uri\":\"https:\\/\\/www.mozilla.com\\/\"}")))
        var exceptionList: List<TrackingProtectionException>? = null

        whenever(session.geckoSession).thenReturn(mockGeckoSession)
        whenever(runtime.contentBlockingController).thenReturn(mockContentBlocking)
        whenever(runtime.contentBlockingController.saveExceptionList()).thenReturn(geckoResult)

        val storage = TrackingProtectionExceptionFileStorage(testContext, runtime)

        storage.fetchAll { exceptions ->
            exceptionList = exceptions
        }
        geckoResult.complete(mockExceptionList)

        verify(mockContentBlocking).saveExceptionList()
        assertTrue(exceptionList!!.isNotEmpty())
        assertEquals("https://www.mozilla.com/", exceptionList!!.first().url)
        assertEquals("eyIxIjp7IjAiOiJodHRwczovL3d3dy5jbm4uY29tLyJ9fQ==", (exceptionList!!.first() as GeckoTrackingProtectionException).principal)

        geckoResult = GeckoResult()
        whenever(runtime.contentBlockingController.saveExceptionList()).thenReturn(geckoResult)

        storage.fetchAll { exceptions ->
            exceptionList = exceptions
        }

        geckoResult.complete(null)
        assertTrue(exceptionList!!.isEmpty())
    }
}
