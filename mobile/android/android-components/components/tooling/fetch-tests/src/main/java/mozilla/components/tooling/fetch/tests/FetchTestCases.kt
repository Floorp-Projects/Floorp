/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.fetch.tests

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.isSuccess
import okhttp3.Headers
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okio.Buffer
import okio.GzipSink
import okio.Okio
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import java.io.File
import java.io.IOException
import java.lang.Exception
import java.net.SocketTimeoutException
import java.util.UUID
import java.util.concurrent.TimeUnit

/**
 * Generic test cases for concept-fetch implementations.
 *
 * We expect any implementation of concept-fetch to pass all test cases here.
 */
@Suppress("IllegalIdentifier", "FunctionName", "unused")
abstract class FetchTestCases {
    /**
     * Creates a new [Client] for running a specific test case with it.
     */
    abstract fun createNewClient(): Client

    /**
     * Creates a new [MockWebServer] to accept test requests.
     */
    open fun createWebServer(): MockWebServer = MockWebServer()

    @Test
    open fun get200WithStringBody() = withServerResponding(
        MockResponse()
            .setBody("Hello World"),
    ) { client ->
        val response = client.fetch(Request(rootUrl()))

        assertEquals(200, response.status)
        assertEquals("Hello World", response.body.string())
    }

    @Test
    open fun get404WithBody() {
        withServerResponding(
            MockResponse()
                .setResponseCode(404)
                .setBody("Error"),
        ) { client ->
            val response = client.fetch(Request(rootUrl()))

            assertEquals(404, response.status)
            assertEquals("Error", response.body.string())
        }
    }

    @Test
    open fun get200WithHeaders() {
        withServerResponding(
            MockResponse(),
        ) { client ->
            val response = client.fetch(
                Request(
                    url = rootUrl(),
                    headers = MutableHeaders()
                        .set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
                        .set("Accept-Encoding", "gzip, deflate")
                        .set("Accept-Language", "en-US,en;q=0.5")
                        .set("Connection", "keep-alive")
                        .set("User-Agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.14; rv:65.0) Gecko/20100101 Firefox/65.0"),
                ),
            )
            assertEquals(200, response.status)

            val request = takeRequest()

            assertTrue(request.headers.size() >= 5)

            val names = request.headers.names()
            assertTrue(names.contains("Accept"))
            assertTrue(names.contains("Accept-Encoding"))
            assertTrue(names.contains("Accept-Language"))
            assertTrue(names.contains("Connection"))
            assertTrue(names.contains("User-Agent"))

            assertEquals(
                "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
                request.headers.get("Accept"),
            )

            assertEquals(
                "gzip, deflate",
                request.headers.get("Accept-Encoding"),
            )

            assertEquals(
                "en-US,en;q=0.5",
                request.headers.get("Accept-Language"),
            )

            assertEquals(
                "keep-alive",
                request.headers.get("Connection"),
            )

            assertEquals(
                "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.14; rv:65.0) Gecko/20100101 Firefox/65.0",
                request.headers.get("User-Agent"),
            )
        }
    }

    @Test
    open fun post200WithBody() {
        withServerResponding(
            MockResponse(),
        ) { client ->
            val response = client.fetch(
                Request(
                    url = rootUrl(),
                    method = Request.Method.POST,
                    body = Request.Body.fromString("Hello World"),
                ),
            )
            assertEquals(200, response.status)

            val request = takeRequest()

            assertEquals("POST", request.method)
            assertEquals("Hello World", request.body.readUtf8())
        }
    }

    @Test
    open fun get200WithGzippedBody() {
        withServerResponding(
            MockResponse()
                .setBody(gzip("This is compressed"))
                .addHeader("Content-Encoding: gzip"),
        ) { client ->
            val response = client.fetch(Request(rootUrl()))
            assertEquals(200, response.status)

            assertEquals("This is compressed", response.body.string())
        }
    }

    @Test
    open fun get302FollowRedirects() {
        withServerResponding(
            MockResponse().setResponseCode(302)
                .addHeader("Location", "/x"),
            MockResponse().setBody("Hello World!"),
        ) { client ->
            val response = client.fetch(
                Request(
                    url = rootUrl(),
                    redirect = Request.Redirect.FOLLOW,
                ),
            )
            assertEquals(200, response.status)

            assertEquals("Hello World!", response.body.string())
        }
    }

    @Test
    open fun get302FollowRedirectsDisabled() {
        withServerResponding(
            MockResponse().setResponseCode(302)
                .addHeader("Location", "/x"),
            MockResponse().setBody("Hello World!"),
        ) { client ->
            val response = client.fetch(
                Request(
                    url = rootUrl(),
                    redirect = Request.Redirect.MANUAL,
                    cookiePolicy = Request.CookiePolicy.OMIT,
                ),
            )
            assertEquals(302, response.status)
        }
    }

    @Test
    open fun get200WithReadTimeout() {
        withServerResponding(
            MockResponse()
                .setBody("Yep!")
                .setBodyDelay(10, TimeUnit.SECONDS),
        ) { client ->
            try {
                val response = client.fetch(
                    Request(url = rootUrl(), readTimeout = Pair(1, TimeUnit.SECONDS)),
                )

                // We're doing this the old-fashioned way instead of using the
                // expected= attribute, because the test is launched on a different
                // thread (using a different coroutine context) than this block.
                fail("Expected read timeout (SocketTimeoutException), but got response: ${response.status}")
            } catch (e: SocketTimeoutException) {
                // expected
            } catch (e: Exception) {
                fail("Expected SocketTimeoutException")
            }
        }
    }

    @Test
    open fun put201FileUpload() {
        val file = File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString())
        file.writer().use { it.write("I am an image file!") }

        withServerResponding(
            MockResponse()
                .setResponseCode(201)
                .setHeader("Location", "/your-image.png")
                .setBody("Thank you!"),
        ) { client ->
            val response = client.fetch(
                Request(
                    url = rootUrl(),
                    method = Request.Method.PUT,
                    headers = MutableHeaders(
                        "Content-Type" to "image/png",
                    ),
                    body = Request.Body.fromFile(file),
                ),
            )

            // Verify response

            assertTrue(response.isSuccess)
            assertEquals(201, response.status)

            assertEquals("Thank you!", response.body.string())

            assertTrue(response.headers.contains("Location"))

            assertEquals("/your-image.png", response.headers.get("Location"))

            // Verify request received by server

            val request = takeRequest()

            assertEquals("PUT", request.method)

            assertEquals("image/png", request.getHeader("Content-Type"))

            assertEquals("I am an image file!", request.body.readUtf8())
        }
    }

    @Test
    open fun get200WithDuplicatedCacheControlResponseHeaders() {
        withServerResponding(
            MockResponse()
                .addHeader("Cache-Control", "no-cache")
                .addHeader("Cache-Control", "no-store")
                .setBody("I am the content"),
        ) { client ->
            val response = client.fetch(Request(rootUrl()))

            response.headers.forEach { (name, value) -> println("$name = $value") }

            assertEquals(200, response.status)
            assertEquals(3, response.headers.size)

            assertEquals("Cache-Control", response.headers[0].name)
            assertEquals("Cache-Control", response.headers[1].name)
            assertEquals("Content-Length", response.headers[2].name)

            assertEquals("no-cache", response.headers[0].value)
            assertEquals("no-store", response.headers[1].value)
            assertEquals("16", response.headers[2].value)

            assertEquals("no-store", response.headers.get("Cache-Control"))
            assertEquals("16", response.headers.get("Content-Length"))
        }
    }

    @Test
    open fun get200WithDuplicatedCacheControlRequestHeaders() {
        withServerResponding(
            MockResponse(),
        ) { client ->
            val response = client.fetch(
                Request(
                    url = rootUrl(),
                    headers = MutableHeaders(
                        "Cache-Control" to "no-cache",
                        "Cache-Control" to "no-store",
                    ),
                ),
            )

            assertEquals(200, response.status)

            val request = takeRequest()

            var cacheHeaders = request.headers.values("Cache-Control")

            assertFalse(cacheHeaders.isEmpty())

            // If multiple headers with the same name are present we accept
            // implementations that *request* a comma-separated list of values
            // as well as those adding additional (duplicated) headers.
            // Technically, comma-separate values are correct:
            // https://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
            // Web servers will understand both representations and for responses
            // we already unify headers across implementations. So, this should
            // be transparent to users.
            if (cacheHeaders[0].contains(",")) {
                cacheHeaders = cacheHeaders[0].split(",")
            }

            assertEquals(2, cacheHeaders.size)
            assertEquals("no-cache", cacheHeaders[0].trim())
            assertEquals("no-store", cacheHeaders[1].trim())
        }
    }

    @Test
    open fun get200OverridingDefaultHeaders() {
        withServerResponding(
            MockResponse(),
        ) { client ->
            val response = client.fetch(
                Request(
                    url = rootUrl(),
                    headers = MutableHeaders(
                        "Accept" to "text/html",
                        "Accept-Encoding" to "deflate",
                        "User-Agent" to "SuperBrowser/1.0",
                        "Connection" to "close",
                    ),
                ),
            )

            assertEquals(200, response.status)

            val request = takeRequest()

            for (i in 0 until request.headers.size()) {
                println(" Header: " + request.headers.name(i) + " = " + request.headers.value(i))
            }

            val acceptHeaders = request.headers.values("Accept")
            assertEquals(1, acceptHeaders.size)
            assertEquals("text/html", acceptHeaders[0])
        }
    }

    @Test
    open fun get200WithCookiePolicy() = withServerResponding(
        MockResponse().addHeader("Set-Cookie", "name=value"),
        MockResponse(),
        MockResponse(),
    ) { client ->

        val responseWithCookies = client.fetch(Request(rootUrl()))
        assertEquals(200, responseWithCookies.status)
        assertEquals("name=value", responseWithCookies.headers["Set-Cookie"])
        assertNull(takeRequest().getHeader("Cookie"))

        // Send additional request, using CookiePolicy.INCLUDE, which should
        // include the cookie set by the previous response.
        val response1 = client.fetch(
            Request(url = rootUrl(), cookiePolicy = Request.CookiePolicy.INCLUDE),
        )

        assertEquals(200, response1.status)
        assertEquals("name=value", takeRequest().getHeader("Cookie"))

        // Send additional request, using CookiePolicy.OMIT, which should
        // NOT include the cookie.
        val response2 = client.fetch(
            Request(url = rootUrl(), cookiePolicy = Request.CookiePolicy.OMIT),
        )

        assertEquals(200, response2.status)
        assertNull(takeRequest().getHeader("Cookie"))
    }

    @Test
    open fun get200WithContentTypeCharset() = withServerResponding(
        MockResponse()
            .addHeader("Content-Type", "text/html; charset=ISO-8859-1")
            .setBody(Buffer().writeString("ÄäÖöÜü", Charsets.ISO_8859_1)),
        MockResponse()
            .addHeader("Content-Type", "text/html; charset=invalid")
            .setBody("Hello World"),
    ) { client ->

        val response = client.fetch(Request(rootUrl()))

        assertEquals(200, response.status)
        assertEquals("ÄäÖöÜü", response.body.string())

        val response2 = client.fetch(Request(rootUrl()))

        assertEquals(200, response2.status)
        assertEquals("Hello World", response2.body.string())
    }

    @Test
    open fun get200WithCacheControl() = withServerResponding(
        MockResponse()
            .addHeader("Cache-Control", "max-age=600")
            .setBody("Cache this!"),
        MockResponse().setBody("Could've cached this!"),
    ) { client ->

        val responseWithCacheControl = client.fetch(Request(rootUrl()))
        assertEquals(200, responseWithCacheControl.status)
        assertEquals("Cache this!", responseWithCacheControl.body.string())
        assertNotNull(responseWithCacheControl.headers["Cache-Control"])

        // Request should hit cache.
        val response1 = client.fetch(Request(rootUrl()))
        assertEquals(200, response1.status)
        assertEquals("Cache this!", response1.body.string())

        // Request should hit network.
        val response2 = client.fetch(Request(rootUrl(), useCaches = false))
        assertEquals(200, response2.status)
        assertEquals("Could've cached this!", response2.body.string())
    }

    @Test
    open fun getThrowsIOExceptionWhenHostNotReachable() {
        try {
            val client = createNewClient()
            val response = client.fetch(Request(url = "http://invalid.offline"))

            // We're doing this the old-fashioned way instead of using the
            // expected= attribute, because the test is launched on a different
            // thread (using a different coroutine context) than this block.
            fail("Expected IOException, but got response: ${response.status}")
        } catch (e: IOException) {
            // expected
        } catch (e: Exception) {
            fail("Expected IOException")
        }
    }

    @Test
    open fun getDataUri() {
        val client = createNewClient()
        val response = client.fetch(Request(url = "data:text/plain;charset=utf-8;base64,SGVsbG8sIFdvcmxkIQ=="))
        assertEquals("13", response.headers["Content-Length"])
        assertEquals("text/plain;charset=utf-8", response.headers["Content-Type"])
        assertEquals("Hello, World!", response.body.string())

        val responseNoCharset = client.fetch(Request(url = "data:text/plain;base64,SGVsbG8sIFdvcmxkIQ=="))
        assertEquals("13", responseNoCharset.headers["Content-Length"])
        assertEquals("text/plain", responseNoCharset.headers["Content-Type"])
        assertEquals("Hello, World!", responseNoCharset.body.string())

        val responseNoContentType = client.fetch(Request(url = "data:;base64,SGVsbG8sIFdvcmxkIQ=="))
        assertEquals("13", responseNoContentType.headers["Content-Length"])
        assertNull(responseNoContentType.headers["Content-Type"])
        assertEquals("Hello, World!", responseNoContentType.body.string())

        val responseNoBase64 = client.fetch(Request(url = "data:text/plain;charset=utf-8,Hello%2C%20World%21"))
        assertEquals("13", responseNoBase64.headers["Content-Length"])
        assertEquals("text/plain;charset=utf-8", responseNoBase64.headers["Content-Type"])
        assertEquals("Hello, World!", responseNoBase64.body.string())
    }

    private inline fun withServerResponding(
        vararg responses: MockResponse,
        crossinline block: MockWebServer.(Client) -> Unit,
    ) {
        val server = createWebServer()

        responses.forEach {
            server.enqueue(it)
        }

        try {
            val client = createNewClient()
            // Subclasses (implementation specific tests) might be instrumented
            // and run on a device so we need to avoid network requests on the
            // main thread.
            runBlocking(Dispatchers.IO) {
                server.start()
                server.block(client)
            }
        } finally {
            try { server.shutdown() } catch (e: IOException) {}
        }
    }

    private fun MockWebServer.rootUrl() = url("/").toString()
}

@Throws(IOException::class)
private fun gzip(data: String): Buffer {
    val result = Buffer()
    val sink = Okio.buffer(GzipSink(result))
    sink.writeUtf8(data)
    sink.close()
    return result
}

private fun Headers.filtered(): Headers {
    val builder = newBuilder()
    ignoredHeaders.forEach { header ->
        builder.removeAll(header)
    }
    return builder.build()
}

// The following headers are getting ignored when verifying headers sent by a Client implementation
private val ignoredHeaders = listOf(
    // GeckoView"s GeckoWebExecutor sends additional "Sec-Fetch-*" headers. Instead of
    // adding those headers to all our implementations, we are just ignoring them in tests.
    "Sec-Fetch-Dest",
    "Sec-Fetch-Mode",
    "Sec-Fetch-Site",
)
