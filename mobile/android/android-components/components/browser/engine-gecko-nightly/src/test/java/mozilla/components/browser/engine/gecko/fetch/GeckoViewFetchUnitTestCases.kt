/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.fetch

import androidx.test.core.app.ApplicationProvider
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.support.test.any
import okhttp3.Headers
import okhttp3.HttpUrl
import okhttp3.mockwebserver.MockWebServer
import okhttp3.mockwebserver.RecordedRequest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyLong
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoWebExecutor
import org.mozilla.geckoview.WebResponse
import org.robolectric.RobolectricTestRunner
import java.io.IOException
import java.nio.ByteBuffer
import java.util.concurrent.TimeoutException

/**
 * We can't run standard JVM unit tests for GWE. Therefore, we provide both
 * instrumented tests as well as these unit tests which mock both requests
 * and responses. While these tests guard our logic to map responses to our
 * concept-fetch abstractions, they are not sufficient to guard the full
 * functionality of [GeckoViewFetchClient]. That's why end-to-end tests are
 * provided in instrumented tests.
 */
@RunWith(RobolectricTestRunner::class)
class GeckoViewFetchUnitTestCases : mozilla.components.tooling.fetch.tests.FetchTestCases() {
    override fun createNewClient(): Client {
        val client = GeckoViewFetchClient(ApplicationProvider.getApplicationContext(), mock(GeckoRuntime::class.java))
        geckoWebExecutor?.let { client.executor = it }
        return client
    }

    override fun createWebServer(): MockWebServer {
        return mockWebServer ?: super.createWebServer()
    }

    private var geckoWebExecutor: GeckoWebExecutor? = null
    private var mockWebServer: MockWebServer? = null

    @Before
    fun setup() {
        geckoWebExecutor = null
    }

    @Test
    fun clientInstance() {
        assertTrue(createNewClient() is GeckoViewFetchClient)
    }

    @Test
    override fun get200WithDefaultHeaders() {
        val server = mock(MockWebServer::class.java)
        `when`(server.url(any())).thenReturn(mock(HttpUrl::class.java))
        val host = server.url("/").host()
        val port = server.url("/").port()
        val headerMap = mapOf(
            "Host" to "$host:$port",
            "Accept" to "*/*",
            "Accept-Language" to "*/*",
            "Accept-Encoding" to "gzip",
            "Connection" to "keep-alive",
            "User-Agent" to "test")
        mockRequest(headerMap)
        mockResponse(200)

        super.get200WithDefaultHeaders()
    }

    @Test
    override fun get200WithDuplicatedCacheControlRequestHeaders() {
        val headerMap = mapOf("Cache-Control" to "no-cache, no-store")
        mockRequest(headerMap)
        mockResponse(200)

        super.get200WithDuplicatedCacheControlRequestHeaders()
    }

    @Test
    override fun get200WithDuplicatedCacheControlResponseHeaders() {
        val responseHeaderMap = mapOf(
            "Cache-Control" to "no-cache, no-store",
            "Content-Length" to "16"
        )
        mockResponse(200, responseHeaderMap)

        super.get200WithDuplicatedCacheControlResponseHeaders()
    }

    @Test
    override fun get200OverridingDefaultHeaders() {
        val headerMap = mapOf(
            "Accept" to "text/html",
            "Accept-Encoding" to "deflate",
            "User-Agent" to "SuperBrowser/1.0",
            "Connection" to "close")
        mockRequest(headerMap)
        mockResponse(200)

        super.get200OverridingDefaultHeaders()
    }

    @Test
    override fun get200WithGzippedBody() {
        val responseHeaderMap = mapOf("Content-Encoding" to "gzip")
        mockRequest()
        mockResponse(200, responseHeaderMap, "This is compressed")

        super.get200WithGzippedBody()
    }

    @Test
    override fun get200WithHeaders() {
        val requestHeaders = mapOf(
            "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Accept-Encoding" to "gzip, deflate",
            "Accept-Language" to "en-US,en;q=0.5",
            "Connection" to "keep-alive",
            "User-Agent" to "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.14; rv:65.0) Gecko/20100101 Firefox/65.0"
        )
        mockRequest(requestHeaders)
        mockResponse(200)

        super.get200WithHeaders()
    }

    @Test
    override fun get200WithReadTimeout() {
        mockRequest()
        mockResponse(200)

        val geckoResult = mock(GeckoResult::class.java)
        `when`(geckoResult.poll(anyLong())).thenThrow(TimeoutException::class.java)
        `when`(geckoWebExecutor!!.fetch(any())).thenReturn(geckoResult as GeckoResult<WebResponse>)

        super.get200WithReadTimeout()
    }

    @Test
    override fun get200WithStringBody() {
        mockRequest()
        mockResponse(200, body = "Hello World")

        super.get200WithStringBody()
    }

    @Test
    override fun get200WithUserAgent() {
        mockRequest(mapOf("User-Agent" to "MozacFetch/"))
        mockResponse(200)
        super.get200WithUserAgent()
    }

    @Test
    @Ignore("Covered by instrumented tests")
    override fun get302FollowRedirects() {
        super.get302FollowRedirects()
    }

    @Test
    @Ignore("Covered by instrumented tests")
    override fun get302FollowRedirectsDisabled() {
        super.get302FollowRedirectsDisabled()
    }

    @Test
    override fun get404WithBody() {
        mockRequest()
        mockResponse(404, body = "Error")
        super.get404WithBody()
    }

    @Test
    override fun post200WithBody() {
        mockRequest(method = "POST", body = "Hello World")
        mockResponse(200)
        super.post200WithBody()
    }

    @Test
    override fun put201FileUpload() {
        mockRequest(method = "PUT", headerMap = mapOf("Content-Type" to "image/png"), body = "I am an image file!")
        mockResponse(201, headerMap = mapOf("Location" to "/your-image.png"), body = "Thank you!")
        super.put201FileUpload()
    }

    @Test(expected = IOException::class)
    fun pollReturningNull() {
        mockResponse(200)

        val geckoResult = mock(GeckoResult::class.java)
        `when`(geckoResult.poll(anyLong())).thenReturn(null)
        `when`(geckoWebExecutor!!.fetch(any())).thenReturn(geckoResult as GeckoResult<WebResponse>)

        val request = mock(Request::class.java)
        `when`(request.url).thenReturn("https://mozilla.org")
        `when`(request.method).thenReturn(Request.Method.GET)
        createNewClient().fetch(request)
    }

    @Test
    fun byteBufferInputStream() {
        val value = "test"
        val buffer = ByteBuffer.allocateDirect(value.length)
        buffer.put(value.toByteArray())
        buffer.rewind()

        var stream = ByteBufferInputStream(buffer)
        assertEquals('t'.toInt(), stream.read())
        assertEquals('e'.toInt(), stream.read())
        assertEquals('s'.toInt(), stream.read())
        assertEquals('t'.toInt(), stream.read())
        assertEquals(-1, stream.read())

        val array = ByteArray(4)
        buffer.rewind()
        stream = ByteBufferInputStream(buffer)
        stream.read(array, 0, 4)
        assertEquals('t'.toByte(), array[0])
        assertEquals('e'.toByte(), array[1])
        assertEquals('s'.toByte(), array[2])
        assertEquals('t'.toByte(), array[3])
    }

    private fun mockRequest(headerMap: Map<String, String>? = null, body: String? = null, method: String = "GET") {
        val server = mock(MockWebServer::class.java)
        `when`(server.url(any())).thenReturn(mock(HttpUrl::class.java))
        val request = mock(RecordedRequest::class.java)
        `when`(request.method).thenReturn(method)

        headerMap?.let {
            `when`(request.headers).thenReturn(Headers.of(headerMap))
            `when`(request.getHeader(any())).thenAnswer { inv -> it[inv.getArgument(0)] }
        }

        body?.let {
            val buffer = okio.Buffer()
            buffer.write(body.toByteArray())
            `when`(request.body).thenReturn(buffer)
        }

        `when`(server.takeRequest()).thenReturn(request)
        mockWebServer = server
    }

    private fun mockResponse(statusCode: Int, headerMap: Map<String, String>? = null, body: String? = null) {
        val executor = mock(GeckoWebExecutor::class.java)
        val builder = WebResponse.Builder("").statusCode(statusCode)
        headerMap?.let {
            headerMap.forEach { (k, v) -> builder.addHeader(k, v) }
        }

        body?.let {
            val buffer = ByteBuffer.allocateDirect(body.length)
            buffer.put(body.toByteArray())
            buffer.rewind()
            builder.body(buffer)
        }

        val response = builder.build()

        `when`(executor.fetch(any())).thenReturn(GeckoResult.fromValue(response))
        geckoWebExecutor = executor
    }
}
