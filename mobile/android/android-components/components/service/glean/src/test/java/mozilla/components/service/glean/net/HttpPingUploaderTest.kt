/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.net

import mozilla.components.service.glean.BuildConfig
import mozilla.components.service.glean.config.Configuration
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.doThrow
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import java.io.IOException
import java.io.OutputStream
import java.net.HttpURLConnection
import java.net.MalformedURLException

@RunWith(RobolectricTestRunner::class)
class HttpPingUploaderTest {
    private val testPath: String = "/some/random/path/not/important"
    private val testPing: String = "{ 'ping': 'test' }"
    private val testDefaultConfig = Configuration(applicationId = "test").copy(
        userAgent = "Glean/Test 25.0.2",
        connectionTimeout = 3050,
        readTimeout = 7050
    )

    @Test
    fun `connection timeouts must be properly set`() {
        val connection = mock<HttpURLConnection>(HttpURLConnection::class.java)

        val client = spy<HttpPingUploader>(HttpPingUploader(testDefaultConfig))

        doReturn(connection).`when`(client).openConnection(anyString(), anyString())
        doReturn(200).`when`(client).doUpload(connection, testPing)

        client.upload(testPath, testPing)

        verify<HttpURLConnection>(connection).readTimeout = 7050
        verify<HttpURLConnection>(connection).connectTimeout = 3050
        verify<HttpURLConnection>(connection, times(1)).disconnect()
    }

    @Test
    fun `user-agent must be properly set`() {
        val connection = mock(HttpURLConnection::class.java)

        val client = spy<HttpPingUploader>(HttpPingUploader(testDefaultConfig))

        doReturn(connection).`when`(client).openConnection(anyString(), anyString())
        doReturn(200).`when`(client).doUpload(connection, testPing)

        client.upload(testPath, testPing)

        verify(connection).setRequestProperty("User-Agent", testDefaultConfig.userAgent)
        verify<HttpURLConnection>(connection, times(1)).disconnect()
    }

    @Test
    fun `X-Client-* headers must be properly set`() {
        val connection = mock(HttpURLConnection::class.java)

        val client = spy<HttpPingUploader>(HttpPingUploader(testDefaultConfig))

        doReturn(connection).`when`(client).openConnection(anyString(), anyString())
        doReturn(200).`when`(client).doUpload(connection, testPing)

        client.upload(testPath, testPing)

        verify(connection).setRequestProperty("X-Client-Type", "Glean")
        verify(connection).setRequestProperty("X-Client-Version", BuildConfig.LIBRARY_VERSION)
        verify<HttpURLConnection>(connection, times(1)).disconnect()
    }

    @Test
    fun `upload() returns true for successful submissions (200)`() {
        val connection = mock(HttpURLConnection::class.java)

        doReturn(200).`when`(connection).responseCode
        doReturn(mock(OutputStream::class.java)).`when`(connection).outputStream

        val client = spy<HttpPingUploader>(HttpPingUploader(testDefaultConfig))
        doReturn(connection).`when`(client).openConnection(anyString(), anyString())

        assertTrue(client.upload(testPath, testPing))
        verify<HttpURLConnection>(connection, times(1)).disconnect()
    }

    @Test
    fun `upload() returns false for server errors (5xx)`() {
        for (responseCode in 500..527) {
            val connection = mock(HttpURLConnection::class.java)

            doReturn(responseCode).`when`(connection).responseCode
            doReturn(mock(OutputStream::class.java)).`when`(connection).outputStream

            val client = spy<HttpPingUploader>(HttpPingUploader(testDefaultConfig))
            doReturn(connection).`when`(client).openConnection(anyString(), anyString())

            assertFalse(client.upload(testPath, testPing))
            verify<HttpURLConnection>(connection, times(1)).disconnect()
        }
    }

    @Test
    fun `upload() returns true for successful submissions (2xx)`() {
        for (responseCode in 200..226) {
            val connection = mock(HttpURLConnection::class.java)

            doReturn(responseCode).`when`(connection).responseCode
            doReturn(mock(OutputStream::class.java)).`when`(connection).outputStream

            val client = spy<HttpPingUploader>(HttpPingUploader(testDefaultConfig))
            doReturn(connection).`when`(client).openConnection(anyString(), anyString())

            assertTrue(client.upload(testPath, testPing))
            verify<HttpURLConnection>(connection, times(1)).disconnect()
        }
    }

    @Test
    fun `upload() returns true for failing submissions with broken requests (4xx)`() {
        for (responseCode in 400..451) {
            val connection = mock(HttpURLConnection::class.java)

            doReturn(responseCode).`when`(connection).responseCode
            doReturn(mock(OutputStream::class.java)).`when`(connection).outputStream

            val client = spy<HttpPingUploader>(HttpPingUploader(testDefaultConfig))
            doReturn(connection).`when`(client).openConnection(anyString(), anyString())

            assertTrue(client.upload(testPath, testPing))
            verify<HttpURLConnection>(connection, times(1)).disconnect()
        }
    }

    @Test
    fun `upload() correctly uploads the ping data`() {
        val server = MockWebServer()
        server.enqueue(MockResponse().setBody("OK"))

        val testConfig = testDefaultConfig.copy(
            userAgent = "Telemetry/42.23",
            serverEndpoint = "http://" + server.hostName + ":" + server.port
        )

        val client = HttpPingUploader(testConfig)
        assertTrue(client.upload(testPath, testPing))

        val request = server.takeRequest()
        assertEquals(testPath, request.path)
        assertEquals("POST", request.method)
        assertEquals(testPing, request.body.readUtf8())
        assertEquals("Telemetry/42.23", request.getHeader("User-Agent"))
        assertEquals("application/json; charset=utf-8", request.getHeader("Content-Type"))

        server.shutdown()
    }

    @Test
    fun `upload() returns true on malformed URLs`() {
        val client = spy<HttpPingUploader>(HttpPingUploader(testDefaultConfig))
        doThrow(MalformedURLException()).`when`(client).openConnection(anyString(), anyString())

        // If the URL is malformed then there's nothing we can do to recover. Therefore this is treated
        // like a successful upload.
        assertTrue(client.upload("path", "ping"))
    }

    @Test
    fun `upload() should return false when upload fails`() {
        val stream = mock(OutputStream::class.java)
        doThrow(IOException()).`when`(stream).write(any(ByteArray::class.java))

        val connection = mock(HttpURLConnection::class.java)
        doReturn(stream).`when`(connection).outputStream

        val client = spy<HttpPingUploader>(HttpPingUploader(testDefaultConfig))
        doReturn(connection).`when`(client).openConnection(anyString(), anyString())

        // And IOException during upload is a failed upload that we should retry. The client should
        // return false in this case.
        assertFalse(client.upload("path", "ping"))
        verify<HttpURLConnection>(connection, times(1)).disconnect()
    }
}