/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.net

import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.lib.fetch.okhttp.OkHttpClient
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.telemetry.glean.config.Configuration
import mozilla.telemetry.glean.net.HttpStatus
import mozilla.telemetry.glean.net.RecoverableFailure
import okhttp3.mockwebserver.Dispatcher
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okhttp3.mockwebserver.RecordedRequest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.robolectric.RobolectricTestRunner
import java.io.IOException
import java.net.CookieHandler
import java.net.CookieManager
import java.net.HttpCookie
import java.net.URI
import java.util.concurrent.TimeUnit

@RunWith(RobolectricTestRunner::class)
class ConceptFetchHttpUploaderTest {
    private val testPath: String = "/some/random/path/not/important"
    private val testPing: String = "{ 'ping': 'test' }"
    private val testDefaultConfig = Configuration()

    /**
     * Create a mock webserver that accepts all requests.
     * @return a [MockWebServer] instance
     */
    private fun getMockWebServer(): MockWebServer {
        val server = MockWebServer()
        server.setDispatcher(
            object : Dispatcher() {
                override fun dispatch(request: RecordedRequest): MockResponse {
                    return MockResponse().setBody("OK")
                }
            },
        )
        return server
    }

    @Test
    fun `connection timeouts must be properly set`() {
        val uploader =
            spy<ConceptFetchHttpUploader>(ConceptFetchHttpUploader(lazy { HttpURLConnectionClient() }))

        val request = uploader.buildRequest(testPath, testPing.toByteArray(), emptyMap())

        assertEquals(
            Pair(ConceptFetchHttpUploader.DEFAULT_READ_TIMEOUT, TimeUnit.MILLISECONDS),
            request.readTimeout,
        )
        assertEquals(
            Pair(ConceptFetchHttpUploader.DEFAULT_CONNECTION_TIMEOUT, TimeUnit.MILLISECONDS),
            request.connectTimeout,
        )
    }

    @Test
    fun `Glean headers are correctly dispatched`() {
        val mockClient: Client = mock()
        `when`(mockClient.fetch(any())).thenReturn(
            Response("URL", 200, mock(), mock()),
        )

        val expectedHeaders = mapOf(
            "Content-Type" to "application/json; charset=utf-8",
            "Test-header" to "SomeValue",
            "OtherHeader" to "Glean/Test 25.0.2",
        )

        val uploader = ConceptFetchHttpUploader(lazy { mockClient })
        uploader.upload(testPath, testPing.toByteArray(), expectedHeaders)
        val requestCaptor = argumentCaptor<Request>()
        verify(mockClient).fetch(requestCaptor.capture())

        expectedHeaders.forEach { (headerName, headerValue) ->
            assertEquals(
                headerValue,
                requestCaptor.value.headers!![headerName],
            )
        }
    }

    @Test
    fun `Cookie policy must be properly set`() {
        val uploader =
            spy<ConceptFetchHttpUploader>(ConceptFetchHttpUploader(lazy { HttpURLConnectionClient() }))

        val request = uploader.buildRequest(testPath, testPing.toByteArray(), emptyMap())

        assertEquals(request.cookiePolicy, Request.CookiePolicy.OMIT)
    }

    @Test
    fun `upload() returns true for successful submissions (200)`() {
        val mockClient: Client = mock()
        `when`(mockClient.fetch(any())).thenReturn(
            Response(
                "URL",
                200,
                mock(),
                mock(),
            ),
        )

        val uploader = spy<ConceptFetchHttpUploader>(ConceptFetchHttpUploader(lazy { mockClient }))

        assertEquals(HttpStatus(200), uploader.upload(testPath, testPing.toByteArray(), emptyMap()))
    }

    @Test
    fun `upload() returns false for server errors (5xx)`() {
        for (responseCode in 500..527) {
            val mockClient: Client = mock()
            `when`(mockClient.fetch(any())).thenReturn(
                Response(
                    "URL",
                    responseCode,
                    mock(),
                    mock(),
                ),
            )

            val uploader = spy<ConceptFetchHttpUploader>(ConceptFetchHttpUploader(lazy { mockClient }))

            assertEquals(HttpStatus(responseCode), uploader.upload(testPath, testPing.toByteArray(), emptyMap()))
        }
    }

    @Test
    fun `upload() returns true for successful submissions (2xx)`() {
        for (responseCode in 200..226) {
            val mockClient: Client = mock()
            `when`(mockClient.fetch(any())).thenReturn(
                Response(
                    "URL",
                    responseCode,
                    mock(),
                    mock(),
                ),
            )

            val uploader = spy<ConceptFetchHttpUploader>(ConceptFetchHttpUploader(lazy { mockClient }))

            assertEquals(HttpStatus(responseCode), uploader.upload(testPath, testPing.toByteArray(), emptyMap()))
        }
    }

    @Test
    fun `upload() returns true for failing submissions with broken requests (4xx)`() {
        for (responseCode in 400..451) {
            val mockClient: Client = mock()
            `when`(mockClient.fetch(any())).thenReturn(
                Response(
                    "URL",
                    responseCode,
                    mock(),
                    mock(),
                ),
            )

            val uploader = spy<ConceptFetchHttpUploader>(ConceptFetchHttpUploader(lazy { mockClient }))

            assertEquals(HttpStatus(responseCode), uploader.upload(testPath, testPing.toByteArray(), emptyMap()))
        }
    }

    @Test
    fun `upload() correctly uploads the ping data with default configuration`() {
        val server = getMockWebServer()

        val client = ConceptFetchHttpUploader(lazy { HttpURLConnectionClient() })

        val submissionUrl = "http://" + server.hostName + ":" + server.port + testPath
        assertEquals(HttpStatus(200), client.upload(submissionUrl, testPing.toByteArray(), mapOf("test" to "header")))

        val request = server.takeRequest()
        assertEquals(testPath, request.path)
        assertEquals("POST", request.method)
        assertEquals(testPing, request.body.readUtf8())
        assertEquals("header", request.getHeader("test"))

        server.shutdown()
    }

    @Test
    fun `upload() correctly uploads the ping data with httpurlconnection client`() {
        val server = getMockWebServer()

        val client = ConceptFetchHttpUploader(lazy { HttpURLConnectionClient() })

        val submissionUrl = "http://" + server.hostName + ":" + server.port + testPath
        assertEquals(HttpStatus(200), client.upload(submissionUrl, testPing.toByteArray(), mapOf("test" to "header")))

        val request = server.takeRequest()
        assertEquals(testPath, request.path)
        assertEquals("POST", request.method)
        assertEquals(testPing, request.body.readUtf8())
        assertEquals("header", request.getHeader("test"))
        assertTrue(request.headers.values("Cookie").isEmpty())

        server.shutdown()
    }

    @Test
    fun `upload() correctly uploads the ping data with OkHttp client`() {
        val server = getMockWebServer()

        val client = ConceptFetchHttpUploader(lazy { OkHttpClient() })

        val submissionUrl = "http://" + server.hostName + ":" + server.port + testPath
        assertEquals(HttpStatus(200), client.upload(submissionUrl, testPing.toByteArray(), mapOf("test" to "header")))

        val request = server.takeRequest()
        assertEquals(testPath, request.path)
        assertEquals("POST", request.method)
        assertEquals(testPing, request.body.readUtf8())
        assertEquals("header", request.getHeader("test"))
        assertTrue(request.headers.values("Cookie").isEmpty())

        server.shutdown()
    }

    @Test
    fun `upload() must not transmit any cookie`() {
        val server = getMockWebServer()

        val testConfig = testDefaultConfig.copy(
            serverEndpoint = "http://localhost:" + server.port,
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
        val client = ConceptFetchHttpUploader(lazy { HttpURLConnectionClient() })
        val submissionUrl = testConfig.serverEndpoint + testPath
        assertEquals(HttpStatus(200), client.upload(submissionUrl, testPing.toByteArray(), emptyMap()))

        val request = server.takeRequest()
        assertEquals(testPath, request.path)
        assertEquals("POST", request.method)
        assertEquals(testPing, request.body.readUtf8())
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

        val uploader = spy<ConceptFetchHttpUploader>(ConceptFetchHttpUploader(lazy { mockClient }))

        // And IOException during upload is a failed upload that we should retry. The client should
        // return false in this case.
        assertEquals(RecoverableFailure(0), uploader.upload("path", "ping".toByteArray(), emptyMap()))
    }

    @Test
    fun `the lazy client should only be instantiated after the first upload`() {
        val mockClient: Client = mock()
        `when`(mockClient.fetch(any())).thenReturn(
            Response("URL", 200, mock(), mock()),
        )
        val uploader = spy<ConceptFetchHttpUploader>(ConceptFetchHttpUploader(lazy { mockClient }))
        assertFalse(uploader.client.isInitialized())

        // After calling upload, the client must get instantiated.
        uploader.upload("path", "ping".toByteArray(), emptyMap())
        assertTrue(uploader.client.isInitialized())
    }

    @Test
    fun `usePrivateRequest sends all requests with private flag`() {
        val mockClient: Client = mock()
        `when`(mockClient.fetch(any())).thenReturn(
            Response("URL", 200, mock(), mock()),
        )

        val expectedHeaders = mapOf(
            "Content-Type" to "application/json; charset=utf-8",
            "Test-header" to "SomeValue",
            "OtherHeader" to "Glean/Test 25.0.2",
        )

        val uploader = ConceptFetchHttpUploader(lazy { mockClient }, true)
        uploader.upload(testPath, testPing.toByteArray(), expectedHeaders)

        val captor = argumentCaptor<Request>()

        verify(mockClient).fetch(captor.capture())

        assertTrue(captor.value.private)
    }
}
