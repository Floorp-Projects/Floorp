/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.net

import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.lib.fetch.okhttp.OkHttpClient
import mozilla.components.service.glean.BuildConfig
import mozilla.components.service.glean.config.Configuration
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.spy
import org.robolectric.RobolectricTestRunner
import java.io.IOException
import java.net.CookieHandler
import java.net.CookieManager
import java.net.HttpCookie
import java.net.URI
import java.util.concurrent.TimeUnit

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
        val uploader = spy<HttpPingUploader>(HttpPingUploader())

        val request = uploader.buildRequest(testPath, testPing, testDefaultConfig)

        assertEquals(Pair(7050L, TimeUnit.MILLISECONDS), request.readTimeout)
        assertEquals(Pair(3050L, TimeUnit.MILLISECONDS), request.connectTimeout)
    }

    @Test
    fun `user-agent must be properly set`() {
        val uploader = spy<HttpPingUploader>(HttpPingUploader())

        val request = uploader.buildRequest(testPath, testPing, testDefaultConfig)

        assertEquals(testDefaultConfig.userAgent, request.headers!!["User-Agent"])
    }

    @Test
    fun `X-Client-* headers must be properly set`() {
        val uploader = spy<HttpPingUploader>(HttpPingUploader())

        val request = uploader.buildRequest(testPath, testPing, testDefaultConfig)

        assertEquals("Glean", request.headers!!["X-Client-Type"])
        assertEquals(BuildConfig.LIBRARY_VERSION, request.headers!!["X-Client-Version"])
    }

    @Test
    fun `Cookie policy must be properly set`() {
        val uploader = spy<HttpPingUploader>(HttpPingUploader())

        val request = uploader.buildRequest(testPath, testPing, testDefaultConfig)

        assertEquals(request.cookiePolicy, Request.CookiePolicy.OMIT)
    }

    @Test
    fun `upload() returns true for successful submissions (200)`() {
        val mockClient: Client = mock()
        `when`(mockClient.fetch(any())).thenReturn(Response(
            "URL", 200, mock(), mock()))

        val uploader = spy<HttpPingUploader>(HttpPingUploader())

        val config = testDefaultConfig.copy(httpClient = mockClient)

        assertTrue(uploader.upload(testPath, testPing, config))
    }
    @Test
    fun `upload() returns false for server errors (5xx)`() {
        for (responseCode in 500..527) {
            val mockClient: Client = mock()
            `when`(mockClient.fetch(any())).thenReturn(Response(
                "URL", responseCode, mock(), mock()))

            val uploader = spy<HttpPingUploader>(HttpPingUploader())

            val config = testDefaultConfig.copy(httpClient = mockClient)

            assertFalse(uploader.upload(testPath, testPing, config))
        }
    }

    @Test
    fun `upload() returns true for successful submissions (2xx)`() {
        for (responseCode in 200..226) {
            val mockClient: Client = mock()
            `when`(mockClient.fetch(any())).thenReturn(Response(
                "URL", responseCode, mock(), mock()))

            val uploader = spy<HttpPingUploader>(HttpPingUploader())

            val config = testDefaultConfig.copy(httpClient = mockClient)

            assertTrue(uploader.upload(testPath, testPing, config))
        }
    }

    @Test
    fun `upload() returns true for failing submissions with broken requests (4xx)`() {
        for (responseCode in 400..451) {
            val mockClient: Client = mock()
            `when`(mockClient.fetch(any())).thenReturn(Response(
                "URL", responseCode, mock(), mock()))

            val uploader = spy<HttpPingUploader>(HttpPingUploader())

            val config = testDefaultConfig.copy(httpClient = mockClient)

            assertTrue(uploader.upload(testPath, testPing, config))
        }
    }

    @Test
    fun `upload() correctly uploads the ping data with default configuration`() {
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
    fun `upload() correctly uploads the ping data with httpurlconnection client`() {
        val server = MockWebServer()
        server.enqueue(MockResponse().setBody("OK"))

        val testConfig = testDefaultConfig.copy(
            userAgent = "Telemetry/42.23",
            serverEndpoint = "http://" + server.hostName + ":" + server.port,
            httpClient = HttpURLConnectionClient()
        )

        val client = HttpPingUploader()
        assertTrue(client.upload(testPath, testPing, testConfig))

        val request = server.takeRequest()
        assertEquals(testPath, request.path)
        assertEquals("POST", request.method)
        assertEquals(testPing, request.body.readUtf8())
        assertEquals("Telemetry/42.23", request.getHeader("User-Agent"))
        assertEquals("application/json; charset=utf-8", request.getHeader("Content-Type"))
        assertTrue(request.headers.values("Cookie").isEmpty())

        server.shutdown()
    }

    @Test
    fun `upload() correctly uploads the ping data with OkHttp client`() {
        val server = MockWebServer()
        server.enqueue(MockResponse().setBody("OK"))

        val testConfig = testDefaultConfig.copy(
            userAgent = "Telemetry/42.23",
            serverEndpoint = "http://" + server.hostName + ":" + server.port,
            httpClient = OkHttpClient()
        )

        val client = HttpPingUploader()
        assertTrue(client.upload(testPath, testPing, testConfig))

        val request = server.takeRequest()
        assertEquals(testPath, request.path)
        assertEquals("POST", request.method)
        assertEquals(testPing, request.body.readUtf8())
        assertEquals("Telemetry/42.23", request.getHeader("User-Agent"))
        assertEquals("application/json; charset=utf-8", request.getHeader("Content-Type"))
        assertTrue(request.headers.values("Cookie").isEmpty())

        server.shutdown()
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
    fun `upload() should return false when upload fails`() {
        val mockClient: Client = mock()
        `when`(mockClient.fetch(any())).thenThrow(IOException())

        val config = testDefaultConfig.copy(httpClient = mockClient)

        val uploader = spy<HttpPingUploader>(HttpPingUploader())

        // And IOException during upload is a failed upload that we should retry. The client should
        // return false in this case.
        assertFalse(uploader.upload("path", "ping", config))
    }
}
