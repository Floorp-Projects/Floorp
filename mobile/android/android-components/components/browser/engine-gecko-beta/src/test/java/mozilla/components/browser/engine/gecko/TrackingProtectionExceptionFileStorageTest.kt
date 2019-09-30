/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.app.Activity
import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import mozilla.components.support.ktx.util.readAndDeserialize
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.json.JSONObject
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mozilla.geckoview.ContentBlockingController
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession
import org.robolectric.Robolectric.buildActivity

@RunWith(AndroidJUnit4::class)
class TrackingProtectionExceptionFileStorageTest {

    private val context: Context
        get() = buildActivity(Activity::class.java).get()

    @Test
    fun `restoreAsync exception`() {
        val mockContentBlocking = mock<ContentBlockingController>()
        val runtime: GeckoRuntime = mock()
        val session = mock<GeckoEngineSession>()
        val geckoResult = GeckoResult<ContentBlockingController.ExceptionList>()
        val mockGeckoSession = mock<GeckoSession>()
        val mockExceptionList = mock<ContentBlockingController.ExceptionList>()

        whenever(session.geckoSession).thenReturn(mockGeckoSession)
        whenever(runtime.contentBlockingController).thenReturn(mockContentBlocking)
        whenever(runtime.contentBlockingController.saveExceptionList()).thenReturn(geckoResult)
        whenever(mockExceptionList.toJson()).thenReturn(JSONObject("{\"principals\":[\"eyIxIjp7IjAiOiJodHRwczovL3d3dy5jbm4uY29tLyJ9fQ==\"],\"uris\":[\"https:\\/\\/www.cnn.com\\/\"]}"))

        val storage = TrackingProtectionExceptionFileStorage(testContext, runtime)
        storage.scope = CoroutineScope(Dispatchers.Main)

        assertNull(storage.getFile(context).readAndDeserialize { })

        storage.add(session)
        geckoResult.complete(mockExceptionList)

        storage.restore()

        verify(mockContentBlocking).restoreExceptionList(any())
        assertNotNull(storage.getFile(context).readAndDeserialize { })
    }

    @Test
    fun `add exception`() {
        val mockContentBlocking = mock<ContentBlockingController>()
        val runtime: GeckoRuntime = mock()
        val session = mock<GeckoEngineSession>()
        val geckoResult = GeckoResult<ContentBlockingController.ExceptionList>()
        val mockGeckoSession = mock<GeckoSession>()
        val mockExceptionList = mock<ContentBlockingController.ExceptionList>()

        whenever(session.geckoSession).thenReturn(mockGeckoSession)
        whenever(runtime.contentBlockingController).thenReturn(mockContentBlocking)
        whenever(runtime.contentBlockingController.saveExceptionList()).thenReturn(geckoResult)
        whenever(mockExceptionList.toJson()).thenReturn(JSONObject("{\"principals\":[\"eyIxIjp7IjAiOiJodHRwczovL3d3dy5jbm4uY29tLyJ9fQ==\"],\"uris\":[\"https:\\/\\/www.cnn.com\\/\"]}"))

        val storage = TrackingProtectionExceptionFileStorage(testContext, runtime)
        storage.scope = CoroutineScope(Dispatchers.Main)

        assertNull(storage.getFile(context).readAndDeserialize { })

        storage.add(session)
        geckoResult.complete(mockExceptionList)

        verify(mockContentBlocking).addException(mockGeckoSession)
        verify(mockContentBlocking).saveExceptionList()
        assertNotNull(storage.getFile(context).readAndDeserialize { })
    }

    @Test
    fun `remove all exceptions`() {
        val mockContentBlocking = mock<ContentBlockingController>()
        val runtime: GeckoRuntime = mock()
        val session = mock<GeckoEngineSession>()
        val geckoResult = GeckoResult<ContentBlockingController.ExceptionList>()
        val mockGeckoSession = mock<GeckoSession>()
        val mockExceptionList = mock<ContentBlockingController.ExceptionList>()

        whenever(session.geckoSession).thenReturn(mockGeckoSession)
        whenever(runtime.contentBlockingController).thenReturn(mockContentBlocking)
        whenever(runtime.contentBlockingController.saveExceptionList()).thenReturn(geckoResult)
        whenever(mockExceptionList.toJson()).thenReturn(JSONObject("{\"principals\":[\"eyIxIjp7IjAiOiJodHRwczovL3d3dy5jbm4uY29tLyJ9fQ==\"],\"uris\":[\"https:\\/\\/www.cnn.com\\/\"]}"))

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
        storage.removeAll()
        verify(mockContentBlocking).clearExceptionList()
        assertNull(storage.getFile(context).readAndDeserialize { })
    }

    @Test
    fun `remove exception`() {
        val mockContentBlocking = mock<ContentBlockingController>()
        val runtime: GeckoRuntime = mock()
        val session = mock<GeckoEngineSession>()
        var geckoResult = GeckoResult<ContentBlockingController.ExceptionList>()
        val mockGeckoSession = mock<GeckoSession>()
        val mockExceptionList = mock<ContentBlockingController.ExceptionList>()

        whenever(session.geckoSession).thenReturn(mockGeckoSession)
        whenever(runtime.contentBlockingController).thenReturn(mockContentBlocking)
        whenever(runtime.contentBlockingController.saveExceptionList()).thenReturn(geckoResult)
        whenever(mockExceptionList.toJson()).thenReturn(JSONObject("{\"principals\":[\"eyIxIjp7IjAiOiJodHRwczovL3d3dy5jbm4uY29tLyJ9fQ==\"],\"uris\":[\"https:\\/\\/www.cnn.com\\/\"]}"))

        // Adding exception
        val storage = TrackingProtectionExceptionFileStorage(testContext, runtime)
        storage.scope = CoroutineScope(Dispatchers.Main)

        assertNull(storage.getFile(context).readAndDeserialize { })

        storage.add(session)
        geckoResult.complete(mockExceptionList)

        verify(mockContentBlocking).addException(mockGeckoSession)
        verify(mockContentBlocking).saveExceptionList()
        assertNotNull(storage.getFile(context).readAndDeserialize { })

        // Removing exception
        geckoResult = GeckoResult()
        whenever(runtime.contentBlockingController.saveExceptionList()).thenReturn(geckoResult)
        storage.remove(session)
        verify(mockContentBlocking).removeException(mockGeckoSession)
        geckoResult.complete(null)
        assertNull(storage.getFile(context).readAndDeserialize { })
    }

    @Test
    fun `contains exception`() {
        val mockContentBlocking = mock<ContentBlockingController>()
        val runtime: GeckoRuntime = mock()
        val session = mock<GeckoEngineSession>()
        var geckoResult = GeckoResult<Boolean>()
        val mockGeckoSession = mock<GeckoSession>()
        var containsException = false
        whenever(session.geckoSession).thenReturn(mockGeckoSession)
        whenever(runtime.contentBlockingController).thenReturn(mockContentBlocking)
        whenever(runtime.contentBlockingController.checkException(mockGeckoSession)).thenReturn(
            geckoResult
        )

        val storage = TrackingProtectionExceptionFileStorage(testContext, runtime)

        storage.contains(session) { contains ->
            containsException = contains
        }
        geckoResult.complete(true)

        verify(mockContentBlocking).checkException(mockGeckoSession)
        assertTrue(containsException)

        geckoResult = GeckoResult()
        whenever(runtime.contentBlockingController.checkException(mockGeckoSession)).thenReturn(
            geckoResult
        )

        storage.contains(session) { contains ->
            containsException = contains
        }
        geckoResult.complete(null)
        assertFalse(containsException)
    }

    @Test
    fun `getAll exceptions`() {
        val mockContentBlocking = mock<ContentBlockingController>()
        val runtime: GeckoRuntime = mock()
        val session = mock<GeckoEngineSession>()
        var geckoResult = GeckoResult<ContentBlockingController.ExceptionList>()
        val mockGeckoSession = mock<GeckoSession>()
        val mockExceptionList = mock<ContentBlockingController.ExceptionList>()
        var exceptionList: List<String>? = null
        whenever(session.geckoSession).thenReturn(mockGeckoSession)
        whenever(runtime.contentBlockingController).thenReturn(mockContentBlocking)
        whenever(runtime.contentBlockingController.saveExceptionList()).thenReturn(geckoResult)
        whenever(mockExceptionList.uris).thenReturn(arrayOf("mozilla.com"))

        val storage = TrackingProtectionExceptionFileStorage(testContext, runtime)

        storage.fetchAll { exceptions ->
            exceptionList = exceptions
        }
        geckoResult.complete(mockExceptionList)

        verify(mockContentBlocking).saveExceptionList()
        assertTrue(exceptionList!!.isNotEmpty())

        geckoResult = GeckoResult()
        whenever(runtime.contentBlockingController.saveExceptionList()).thenReturn(geckoResult)

        storage.fetchAll { exceptions ->
            exceptionList = exceptions
        }

        geckoResult.complete(null)
        assertTrue(exceptionList!!.isEmpty())
    }
}
