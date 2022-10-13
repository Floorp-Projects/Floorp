/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.fetch

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import mozilla.components.tooling.fetch.tests.FetchTestCases
import okhttp3.Headers
import okhttp3.mockwebserver.MockWebServer
import okhttp3.mockwebserver.RecordedRequest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentCaptor
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyLong
import org.mockito.Mockito.verify
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoWebExecutor
import org.mozilla.geckoview.WebRequest
import org.mozilla.geckoview.WebRequestError
import org.mozilla.geckoview.WebResponse
import java.io.IOException
import java.nio.charset.Charset
import java.util.concurrent.TimeoutException

/**
 * We can't run standard JVM unit tests for GWE. Therefore, we provide both
 * instrumented tests as well as these unit tests which mock both requests
 * and responses. While these tests guard our logic to map responses to our
 * concept-fetch abstractions, they are not sufficient to guard the full
 * functionality of [GeckoViewFetchClient]. That's why end-to-end tests are
 * provided in instrumented tests.
 */
@RunWith(AndroidJUnit4::class)
class GeckoViewFetchUnitTestCases : FetchTestCases() {

    override fun createNewClient(): Client {
        val client = GeckoViewFetchClient(testContext, mock())
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
    @Ignore("With Java 11 Mockito can't mock MockWebServer classes")
    override fun get200WithDuplicatedCacheControlRequestHeaders() {
        val headerMap = mapOf("Cache-Control" to "no-cache, no-store")
        mockRequest(headerMap)
        mockResponse(200)

        super.get200WithDuplicatedCacheControlRequestHeaders()
    }

    @Test
    @Ignore("With Java 11 Mockito can't mock MockWebServer classes")
    override fun get200WithDuplicatedCacheControlResponseHeaders() {
        val responseHeaderMap = mapOf(
            "Cache-Control" to "no-cache, no-store",
            "Content-Length" to "16",
        )
        mockResponse(200, responseHeaderMap)

        super.get200WithDuplicatedCacheControlResponseHeaders()
    }

    @Test
    @Ignore("With Java 11 Mockito can't mock MockWebServer classes")
    override fun get200OverridingDefaultHeaders() {
        val headerMap = mapOf(
            "Accept" to "text/html",
            "Accept-Encoding" to "deflate",
            "User-Agent" to "SuperBrowser/1.0",
            "Connection" to "close",
        )
        mockRequest(headerMap)
        mockResponse(200)

        super.get200OverridingDefaultHeaders()
    }

    @Test
    @Ignore("With Java 11 Mockito can't mock MockWebServer classes")
    override fun get200WithGzippedBody() {
        val responseHeaderMap = mapOf("Content-Encoding" to "gzip")
        mockRequest()
        mockResponse(200, responseHeaderMap, "This is compressed")

        super.get200WithGzippedBody()
    }

    @Test
    @Ignore("With Java 11 Mockito can't mock MockWebServer classes")
    override fun get200WithHeaders() {
        val requestHeaders = mapOf(
            "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Accept-Encoding" to "gzip, deflate",
            "Accept-Language" to "en-US,en;q=0.5",
            "Connection" to "keep-alive",
            "User-Agent" to "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.14; rv:65.0) Gecko/20100101 Firefox/65.0",
        )
        mockRequest(requestHeaders)
        mockResponse(200)

        super.get200WithHeaders()
    }

    @Test
    @Ignore("With Java 11 Mockito can't mock MockWebServer classes")
    override fun get200WithReadTimeout() {
        mockRequest()
        mockResponse(200)

        val geckoResult = mock<GeckoResult<*>>()
        whenever(geckoResult.poll(anyLong())).thenThrow(TimeoutException::class.java)
        @Suppress("UNCHECKED_CAST")
        whenever(geckoWebExecutor!!.fetch(any(), anyInt())).thenReturn(geckoResult as GeckoResult<WebResponse>)

        super.get200WithReadTimeout()
    }

    @Test
    @Ignore("With Java 11 Mockito can't mock MockWebServer classes")
    override fun get200WithStringBody() {
        mockRequest()
        mockResponse(200, body = "Hello World")

        super.get200WithStringBody()
    }

    @Test
    override fun get302FollowRedirects() {
        mockResponse(200)

        val request = mock<Request>()
        whenever(request.url).thenReturn("https://mozilla.org")
        whenever(request.method).thenReturn(Request.Method.GET)
        whenever(request.redirect).thenReturn(Request.Redirect.FOLLOW)
        createNewClient().fetch(request)

        verify(geckoWebExecutor)!!.fetch(any(), eq(GeckoWebExecutor.FETCH_FLAGS_NONE))
    }

    @Test
    override fun get302FollowRedirectsDisabled() {
        mockResponse(200)

        val request = mock<Request>()
        whenever(request.url).thenReturn("https://mozilla.org")
        whenever(request.method).thenReturn(Request.Method.GET)
        whenever(request.redirect).thenReturn(Request.Redirect.MANUAL)
        createNewClient().fetch(request)

        verify(geckoWebExecutor)!!.fetch(any(), eq(GeckoWebExecutor.FETCH_FLAGS_NO_REDIRECTS))
    }

    @Test
    @Ignore("With Java 11 Mockito can't mock MockWebServer classes")
    override fun get404WithBody() {
        mockRequest()
        mockResponse(404, body = "Error")
        super.get404WithBody()
    }

    @Test
    @Ignore("With Java 11 Mockito can't mock MockWebServer classes")
    override fun post200WithBody() {
        mockRequest(method = "POST", body = "Hello World")
        mockResponse(200)
        super.post200WithBody()
    }

    @Test
    @Ignore("With Java 11 Mockito can't mock MockWebServer classes")
    override fun put201FileUpload() {
        mockRequest(method = "PUT", headerMap = mapOf("Content-Type" to "image/png"), body = "I am an image file!")
        mockResponse(201, headerMap = mapOf("Location" to "/your-image.png"), body = "Thank you!")
        super.put201FileUpload()
    }

    @Test(expected = IOException::class)
    fun pollReturningNull() {
        mockResponse(200)

        val geckoResult = mock<GeckoResult<*>>()
        whenever(geckoResult.poll(anyLong())).thenReturn(null)
        @Suppress("UNCHECKED_CAST")
        whenever(geckoWebExecutor!!.fetch(any(), anyInt())).thenReturn(geckoResult as GeckoResult<WebResponse>)

        val request = mock<Request>()
        whenever(request.url).thenReturn("https://mozilla.org")
        whenever(request.method).thenReturn(Request.Method.GET)
        createNewClient().fetch(request)
    }

    @Test
    override fun get200WithCookiePolicy() {
        mockResponse(200)

        val request = mock<Request>()
        whenever(request.url).thenReturn("https://mozilla.org")
        whenever(request.method).thenReturn(Request.Method.GET)
        whenever(request.cookiePolicy).thenReturn(Request.CookiePolicy.OMIT)
        createNewClient().fetch(request)

        verify(geckoWebExecutor)!!.fetch(any(), eq(GeckoWebExecutor.FETCH_FLAGS_ANONYMOUS))
    }

    @Test
    fun performPrivateRequest() {
        mockResponse(200)

        val request = mock<Request>()
        whenever(request.url).thenReturn("https://mozilla.org")
        whenever(request.method).thenReturn(Request.Method.GET)
        whenever(request.private).thenReturn(true)
        createNewClient().fetch(request)

        verify(geckoWebExecutor)!!.fetch(any(), eq(GeckoWebExecutor.FETCH_FLAGS_PRIVATE))
    }

    @Test
    override fun get200WithContentTypeCharset() {
        val request = mock<Request>()
        whenever(request.url).thenReturn("https://mozilla.org")
        whenever(request.method).thenReturn(Request.Method.GET)

        mockResponse(
            200,
            headerMap = mapOf("Content-Type" to "text/html; charset=ISO-8859-1"),
            body = "ÄäÖöÜü",
            charset = Charsets.ISO_8859_1,
        )

        val response = createNewClient().fetch(request)
        assertEquals("ÄäÖöÜü", response.body.string())
    }

    @Test
    override fun get200WithCacheControl() {
        mockResponse(200)

        val request = mock<Request>()
        whenever(request.url).thenReturn("https://mozilla.org")
        whenever(request.method).thenReturn(Request.Method.GET)
        whenever(request.useCaches).thenReturn(false)
        createNewClient().fetch(request)

        val captor = ArgumentCaptor.forClass(WebRequest::class.java)

        verify(geckoWebExecutor)!!.fetch(captor.capture(), eq(GeckoWebExecutor.FETCH_FLAGS_NONE))
        assertEquals(WebRequest.CACHE_MODE_RELOAD, captor.value.cacheMode)
    }

    @Test(expected = IOException::class)
    override fun getThrowsIOExceptionWhenHostNotReachable() {
        val executor = mock<GeckoWebExecutor>()
        whenever(executor.fetch(any(), anyInt())).thenAnswer { throw WebRequestError(0, 0) }
        geckoWebExecutor = executor

        createNewClient().fetch(Request(""))
    }

    @Test
    fun toResponseMustReturn200ForBlobUrls() {
        val builder = WebResponse.Builder("blob:https://mdn.mozillademos.org/d518464c-5075-9046-aef2-9c313214ed53").statusCode(0).build()
        assertEquals(Response.SUCCESS, builder.toResponse().status)
    }

    @Test
    fun toResponseMustReturn200ForDataUrls() {
        val builder = WebResponse.Builder("data:,Hello%2C%20World!").statusCode(0).build()
        assertEquals(Response.SUCCESS, builder.toResponse().status)
    }

    private fun mockRequest(headerMap: Map<String, String>? = null, body: String? = null, method: String = "GET") {
        val server = mock<MockWebServer>()
        whenever(server.url(any())).thenReturn(mock())
        val request = mock<RecordedRequest>()
        whenever(request.method).thenReturn(method)

        headerMap?.let {
            whenever(request.headers).thenReturn(Headers.of(headerMap))
            whenever(request.getHeader(any())).thenAnswer { inv -> it[inv.getArgument(0)] }
        }

        body?.let {
            val buffer = okio.Buffer()
            buffer.write(body.toByteArray())
            whenever(request.body).thenReturn(buffer)
        }

        whenever(server.takeRequest()).thenReturn(request)
        mockWebServer = server
    }

    private fun mockResponse(
        statusCode: Int,
        headerMap: Map<String, String>? = null,
        body: String? = null,
        charset: Charset = Charsets.UTF_8,
    ) {
        val executor = mock<GeckoWebExecutor>()
        val builder = WebResponse.Builder("").statusCode(statusCode)
        headerMap?.let {
            headerMap.forEach { (k, v) -> builder.addHeader(k, v) }
        }

        body?.let {
            builder.body(it.byteInputStream(charset))
        }

        val response = builder.build()

        whenever(executor.fetch(any(), anyInt())).thenReturn(GeckoResult.fromValue(response))
        geckoWebExecutor = executor
    }
}
