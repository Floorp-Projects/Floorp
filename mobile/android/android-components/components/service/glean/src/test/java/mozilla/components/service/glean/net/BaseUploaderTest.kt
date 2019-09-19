/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.net

import mozilla.components.service.glean.BuildConfig
import mozilla.components.service.glean.config.Configuration
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import java.util.Calendar
import java.util.TimeZone

@RunWith(RobolectricTestRunner::class)
class BaseUploaderTest {
    private val testPath: String = "/some/random/path/not/important"
    private val testPing: String = "{ 'ping': 'test' }"
    private val testDefaultConfig = Configuration().copy(
        userAgent = "Glean/Test 25.0.2"
    )

    /**
     * A stub uploader class that does not upload anything.
     */
    private class TestUploader : PingUploader {
        override fun upload(url: String, data: String, headers: HeadersList): Boolean {
            return false
        }
    }

    @Test
    fun `upload() must get called with the full submission path`() {
        val uploader = spy<BaseUploader>(BaseUploader(TestUploader()))

        val expectedUrl = testDefaultConfig.serverEndpoint + testPath
        uploader.doUpload(testPath, testPing, testDefaultConfig)
        verify(uploader).upload(eq(expectedUrl), any(), any())
    }

    @Test
    fun `All headers are correctly reported for upload`() {
        val uploader = spy<BaseUploader>(BaseUploader(TestUploader()))
        `when`(uploader.getCalendarInstance()).thenAnswer {
            val fakeNow = Calendar.getInstance()
            fakeNow.clear()
            fakeNow.timeZone = TimeZone.getTimeZone("GMT")
            fakeNow.set(2015, 6, 11, 11, 0, 0)
            fakeNow
        }

        uploader.doUpload(testPath, testPing, testDefaultConfig)
        val headersCaptor = argumentCaptor<HeadersList>()

        val expectedUrl = testDefaultConfig.serverEndpoint + testPath
        verify(uploader).upload(eq(expectedUrl), eq(testPing), headersCaptor.capture())

        val expectedHeaders = mapOf(
            "Content-Type" to "application/json; charset=utf-8",
            "Date" to "Sat, 11 Jul 2015 11:00:00 GMT",
            "User-Agent" to "Glean/Test 25.0.2",
            "X-Client-Type" to "Glean",
            "X-Client-Version" to BuildConfig.LIBRARY_VERSION
        )

        expectedHeaders.forEach { (headerName, headerValue) ->
            assertEquals(
                headerValue,
                headersCaptor.value.find { it.first == headerName }!!.second
            )
        }
    }

    @Test
    fun `X-Debug-ID header is correctly added when pingTag is not null`() {
        val uploader = spy<BaseUploader>(BaseUploader(TestUploader()))

        val debugConfig = testDefaultConfig.copy(
            pingTag = "this-ping-is-tagged"
        )

        uploader.doUpload(testPath, testPing, debugConfig)
        val headersCaptor = argumentCaptor<HeadersList>()
        verify(uploader).upload(any(), any(), headersCaptor.capture())

        assertEquals(
            "this-ping-is-tagged",
            headersCaptor.value.find { it.first == "X-Debug-ID" }!!.second
        )
    }
}
