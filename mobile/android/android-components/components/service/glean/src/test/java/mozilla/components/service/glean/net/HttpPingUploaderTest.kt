/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.net

import mozilla.components.service.glean.BuildConfig
import mozilla.components.service.glean.config.Configuration
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.Test
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue

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
import java.net.CookieHandler
import java.net.CookieManager
import java.net.HttpCookie
import java.net.HttpURLConnection
import java.net.MalformedURLException
import java.net.URI

@RunWith(RobolectricTestRunner::class)
class HttpPingUploaderTest {
    private val testPath: String = "/some/random/path/not/important"
    private val testPing: String = "{ 'ping': 'test' }"
    private val testDefaultConfig = Configuration().copy(
        userAgent = "Glean/Test 25.0.2",
        connectionTimeout = 3050,
        readTimeout = 7050
    )

    @Test
    fun `connection timeouts must be properly set`() {
        val connection = mock<HttpURLConnection>(HttpURLConnection::class.java)

        val client = spy<HttpPingUploader>(HttpPingUploader())

        doReturn(connection).`when`(client).openConnection(anyString(), anyString())
        doReturn(200).`when`(client).doUpload(connection, testPing)

        client.upload(testPath, testPing, testDefaultConfig)

        verify<HttpURLConnection>(connection).readTimeout = 7050
        verify<HttpURLConnection>(connection).connectTimeout = 3050
        verify<HttpURLConnection>(connection, times(1)).disconnect()
    }

    @Test
    fun `user-agent must be properly set`() {
        val connection = mock(HttpURLConnection::class.java)

        val client = spy<HttpPingUploader>(HttpPingUploader())

        doReturn(connection).`when`(client).openConnection(anyString(), anyString())
        doReturn(200).`when`(client).doUpload(connection, testPing)

        client.upload(testPath, testPing, testDefaultConfig)

        verify(connection).setRequestProperty("User-Agent", testDefaultConfig.userAgent)
        verify<HttpURLConnection>(connection, times(1)).disconnect()
    }

    @Test
    fun `X-Client-* headers must be properly set`() {
        val connection = mock(HttpURLConnection::class.java)

        val client = spy<HttpPingUploader>(HttpPingUploader())

        doReturn(connection).`when`(client).openConnection(anyString(), anyString())
        doReturn(200).`when`(client).doUpload(connection, testPing)

        client.upload(testPath, testPing, testDefaultConfig)

        verify(connection).setRequestProperty("X-Client-Type", "Glean")
        verify(connection).setRequestProperty("X-Client-Version", BuildConfig.LIBRARY_VERSION)
        verify<HttpURLConnection>(connection, times(1)).disconnect()
    }

    @Test
    fun `upload() returns true for successful submissions (200)`() {
        val connection = mock(HttpURLConnection::class.java)

        doReturn(200).`when`(connection).responseCode
        doReturn(mock(OutputStream::class.java)).`when`(connection).outputStream

        val client = spy<HttpPingUploader>(HttpPingUploader())
        doReturn(connection).`when`(client).openConnection(anyString(), anyString())

        assertTrue(client.upload(testPath, testPing, testDefaultConfig))
        verify<HttpURLConnection>(connection, times(1)).disconnect()
    }

    @Test
    fun `upload() returns false for server errors (5xx)`() {
        for (responseCode in 500..527) {
            val connection = mock(HttpURLConnection::class.java)

            doReturn(responseCode).`when`(connection).responseCode
            doReturn(mock(OutputStream::class.java)).`when`(connection).outputStream

            val client = spy<HttpPingUploader>(HttpPingUploader())
            doReturn(connection).`when`(client).openConnection(anyString(), anyString())

            assertFalse(client.upload(testPath, testPing, testDefaultConfig))
            verify<HttpURLConnection>(connection, times(1)).disconnect()
        }
    }

    @Test
    fun `upload() returns true for successful submissions (2xx)`() {
        for (responseCode in 200..226) {
            val connection = mock(HttpURLConnection::class.java)

            doReturn(responseCode).`when`(connection).responseCode
            doReturn(mock(OutputStream::class.java)).`when`(connection).outputStream

            val client = spy<HttpPingUploader>(HttpPingUploader())
            doReturn(connection).`when`(client).openConnection(anyString(), anyString())

            assertTrue(client.upload(testPath, testPing, testDefaultConfig))
            verify<HttpURLConnection>(connection, times(1)).disconnect()
        }
    }

    @Test
    fun `upload() returns true for failing submissions with broken requests (4xx)`() {
        for (responseCode in 400..451) {
            val connection = mock(HttpURLConnection::class.java)

            doReturn(responseCode).`when`(connection).responseCode
            doReturn(mock(OutputStream::class.java)).`when`(connection).outputStream

            val client = spy<HttpPingUploader>(HttpPingUploader())
            doReturn(connection).`when`(client).openConnection(anyString(), anyString())

            assertTrue(client.upload(testPath, testPing, testDefaultConfig))
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

        val client = HttpPingUploader()
        assertTrue(client.upload(testPath, testPing, testConfig))

        val request = server.takeRequest()
        assertEquals(testPath, request.path)
        assertEquals("POST", request.method)
        assertEquals(testPing, request.body.readUtf8())
        assertEquals("Telemetry/42.23", request.getHeader("User-Agent"))
        assertEquals("application/json; charset=utf-8", request.getHeader("Content-Type"))

        server.shutdown()
    }

    @Test
    fun `removeCookies() must not throw for malformed URLs`() {
        val client = HttpPingUploader()
        CookieHandler.setDefault(CookieManager())
        client.removeCookies("lolprotocol://definitely-not-valid,")
    }

    @Test
    fun `upload() must not transmit any cookie`() {
        val server = MockWebServer()
        server.enqueue(MockResponse().setBody("OK"))

        val testConfig = testDefaultConfig.copy(
            userAgent = "Telemetry/42.23",
            serverEndpoint = "http://localhost:" + server.port
        )

        // Set the default cookie manager/handler to be used for the http upload.
        val cookieManager = CookieManager()
        CookieHandler.setDefault(cookieManager)

        // Store a sample cookie.
        val cookie = HttpCookie("cookie-time", "yes")
        cookie.domain = testConfig.serverEndpoint
        cookie.path = testPath
        cookie.version = 0
        cookieManager.cookieStore.add(URI(testConfig.serverEndpoint), cookie)

        // Store a cookie for a subdomain of the same domain's as the server endpoint,
        // to make sure we don't accidentally remove it.
        val cookie2 = HttpCookie("cookie-time2", "yes")
        cookie2.domain = "sub.localhost"
        cookie2.path = testPath
        cookie2.version = 0
        cookieManager.cookieStore.add(URI("http://sub.localhost:${server.port}/test"), cookie2)

        // Add another cookie for the same domain. This one should be removed as well.
        val cookie3 = HttpCookie("cookie-time3", "yes")
        cookie3.domain = "localhost"
        cookie3.path = testPath
        cookie3.version = 0
        cookieManager.cookieStore.add(URI("http://localhost:${server.port}/test"), cookie3)

        // Trigger the connection.
        val client = HttpPingUploader()
        assertTrue(client.upload(testPath, testPing, testConfig))

        val request = server.takeRequest()
        assertEquals(testPath, request.path)
        assertEquals("POST", request.method)
        assertEquals(testPing, request.body.readUtf8())
        assertEquals("Telemetry/42.23", request.getHeader("User-Agent"))
        assertEquals("application/json; charset=utf-8", request.getHeader("Content-Type"))
        assertTrue(request.headers.values("Cookie").isEmpty())

        // Check that we still have a cookie.
        assertEquals(1, cookieManager.cookieStore.cookies.size)
        assertEquals("cookie-time2", cookieManager.cookieStore.cookies[0].name)

        server.shutdown()
    }

    @Test
    fun `upload() returns true on malformed URLs`() {
        val client = spy<HttpPingUploader>(HttpPingUploader())
        doThrow(MalformedURLException()).`when`(client).openConnection(anyString(), anyString())

        // If the URL is malformed then there's nothing we can do to recover. Therefore this is treated
        // like a successful upload.
        assertTrue(client.upload("path", "ping", testDefaultConfig))
    }

    @Test
    fun `upload() should return false when upload fails`() {
        val stream = mock(OutputStream::class.java)
        doThrow(IOException()).`when`(stream).write(any(ByteArray::class.java))

        val connection = mock(HttpURLConnection::class.java)
        doReturn(stream).`when`(connection).outputStream

        val client = spy<HttpPingUploader>(HttpPingUploader())
        doReturn(connection).`when`(client).openConnection(anyString(), anyString())

        // And IOException during upload is a failed upload that we should retry. The client should
        // return false in this case.
        assertFalse(client.upload("path", "ping", testDefaultConfig))
        verify<HttpURLConnection>(connection, times(1)).disconnect()
    }
}