/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.fetch.tests

import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.success
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okio.Buffer
import okio.GzipSink
import okio.Okio
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import java.io.File
import java.io.IOException
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

    @Test
    fun `GET (200) with String body`() = withServerResponding(
        MockResponse()
            .setBody("Hello World")
    ) { client ->
        val response = client.fetch(Request(rootUrl()))

        assertEquals(200, response.status)
        assertEquals("Hello World", response.body.string())
    }

    @Test
    fun ` GET (404) with body`() {
        withServerResponding(
            MockResponse()
                .setResponseCode(404)
                .setBody("Error")
        ) { client ->
            val response = client.fetch(Request(rootUrl()))

            assertEquals(404, response.status)
            assertEquals("Error", response.body.string())
        }
    }

    @Test
    fun `GET (200) default headers`() {
        withServerResponding(
            MockResponse()
        ) { client ->
            val response = client.fetch(Request(rootUrl()))
            assertEquals(200, response.status)

            val request = takeRequest()

            for (i in 0 until request.headers.size()) {
                println(request.headers.name(i) + " = " + request.headers.value(i))
            }

            assertEquals(5, request.headers.size())

            val names = request.headers.names()
            assertTrue(names.contains("Host"))
            assertTrue(names.contains("User-Agent"))
            assertTrue(names.contains("Connection"))
            assertTrue(names.contains("Accept-Encoding"))
            assertTrue(names.contains("Accept"))

            val host = url("/").host()
            val port = url("/").port()
            assertEquals("$host:$port", request.getHeader("Host"))

            assertEquals("*/*", request.getHeader("Accept"))

            assertEquals("gzip", request.getHeader("Accept-Encoding"))

            // Ignoring case here: okhttp uses "Keep-Alive" and httpurlconnection uses "keep-alive".
            // I do not want to override the header of either because I do not know if they read it
            // internally and require a certain case.
            assertEquals("keep-alive", request.getHeader("Connection").toLowerCase())
        }
    }

    @Test
    fun `GET (200) with headers`() {
        withServerResponding(
            MockResponse()
        ) { client ->
            val response = client.fetch(Request(
                url = rootUrl(),
                headers = MutableHeaders()
                    .set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
                    .set("Accept-Encoding", "gzip, deflate")
                    .set("Accept-Language", "en-US,en;q=0.5")
                    .set("Connection", "keep-alive")
                    .set("User-Agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.14; rv:65.0) Gecko/20100101 Firefox/65.0")
            ))
            assertEquals(200, response.status)

            val request = takeRequest()

            assertTrue(request.headers.size() >= 5)

            val names = request.headers.names()
            assertTrue(names.contains("Accept"))
            assertTrue(names.contains("Accept-Encoding"))
            assertTrue(names.contains("Accept-Language"))
            assertTrue(names.contains("Connection"))
            assertTrue(names.contains("User-Agent"))

            assertEquals("text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
                request.headers.get("Accept"))

            assertEquals("gzip, deflate",
                request.headers.get("Accept-Encoding"))

            assertEquals("en-US,en;q=0.5",
                request.headers.get("Accept-Language"))

            assertEquals("keep-alive",
                request.headers.get("Connection"))

            assertEquals("Mozilla/5.0 (Macintosh; Intel Mac OS X 10.14; rv:65.0) Gecko/20100101 Firefox/65.0",
                request.headers.get("User-Agent"))
        }
    }

    @Test
    fun `POST (200) with body`() {
        withServerResponding(
            MockResponse()
        ) { client ->
            val response = client.fetch(Request(
                url = rootUrl(),
                method = Request.Method.POST,
                body = Request.Body.fromString("Hello World")
            ))
            assertEquals(200, response.status)

            val request = takeRequest()

            assertEquals("POST", request.method)
            assertEquals("Hello World", request.body.readUtf8())
        }
    }

    @Test
    fun `GET (200) user agent`() {
        withServerResponding(
            MockResponse()
        ) { client ->
            val response = client.fetch(Request(rootUrl()))
            assertEquals(200, response.status)

            val request = takeRequest()
            val names = request.headers.names()

            assertTrue(names.contains("User-Agent"))

            val userAgent = request.headers.get("User-Agent")
            assertTrue(userAgent!!.startsWith("MozacFetch/"))
        }
    }

    @Test
    fun `GET (200) gzipped body`() {
        withServerResponding(
            MockResponse()
                .setBody(gzip("This is compressed"))
                .addHeader("Content-Encoding: gzip")
        ) { client ->
            val response = client.fetch(Request(rootUrl()))
            assertEquals(200, response.status)

            assertEquals("This is compressed", response.body.string())
        }
    }

    @Test
    fun `GET (302, 200) follow redirects`() {
        withServerResponding(
            MockResponse().setResponseCode(302)
                .addHeader("Location", "/x"),
            MockResponse().setBody("Hello World!")
        ) { client ->
            val response = client.fetch(
                Request(
                    url = rootUrl(),
                    redirect = Request.Redirect.FOLLOW))
            assertEquals(200, response.status)

            assertEquals("Hello World!", response.body.string())
        }
    }

    @Test
    fun `GET (302) follow redirects disabled`() {
        withServerResponding(
            MockResponse().setResponseCode(302)
                .addHeader("Location", "/x"),
            MockResponse().setBody("Hello World!")
        ) { client ->
            val response = client.fetch(
                Request(
                    url = rootUrl(),
                    redirect = Request.Redirect.MANUAL))
            assertEquals(302, response.status)
        }
    }

    @Test(expected = SocketTimeoutException::class)
    fun `GET (200) with read timeout`() {
        withServerResponding(
            MockResponse()
                .setBody("Yep!")
                .setBodyDelay(10, TimeUnit.SECONDS)
        ) { client ->
            val response = client.fetch(
                Request(
                    url = rootUrl(),
                    readTimeout = Pair(1, TimeUnit.SECONDS)))

            fail("Expected read timeout, but got response: ${response.status}")
        }
    }

    @Test
    fun `PUT (201) file upload`() {
        val file = File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString())
        file.writer().use { it.write("I am an image file!") }

        withServerResponding(
            MockResponse()
                .setResponseCode(201)
                .setHeader("Location", "/your-image.png")
                .setBody("Thank you!")
        ) { client ->
            val response = client.fetch(Request(
                url = rootUrl(),
                method = Request.Method.PUT,
                headers = MutableHeaders(
                    "Content-Type" to "image/png"
                ),
                body = Request.Body.fromFile(file)
            ))

            // Verify response

            assertTrue(response.success)
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
    fun `GET (200) duplicated cache control response headers`() {
        withServerResponding(
            MockResponse()
                .addHeader("Cache-Control", "no-cache")
                .addHeader("Cache-Control", "no-store")
                .setBody("I am the content")
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
    fun `GET (200) with duplicated cache control request headers`() {
        withServerResponding(
            MockResponse()
        ) { client ->
            val response = client.fetch(
                Request(
                    url = rootUrl(),
                    headers = MutableHeaders(
                        "Cache-Control" to "no-cache",
                        "Cache-Control" to "no-store")))

            assertEquals(200, response.status)

            val request = takeRequest()

            val cacheHeaders = request.headers.values("Cache-Control")
            assertEquals(2, cacheHeaders.size)
            assertEquals("no-cache", cacheHeaders[0])
            assertEquals("no-store", cacheHeaders[1])
        }
    }

    @Test
    fun `GET (200) Overriding default headers`() {
        withServerResponding(
            MockResponse()
        ) { client ->
            val response = client.fetch(
                Request(
                    url = rootUrl(),
                    headers = MutableHeaders(
                        "Accept" to "text/html",
                        "Accept-Encoding" to "deflate",
                        "User-Agent" to "SuperBrowser/1.0",
                        "Connection" to "close")))

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

    private inline fun withServerResponding(vararg responses: MockResponse, block: MockWebServer.(Client) -> Unit) {
        val server = MockWebServer()

        responses.forEach {
            server.enqueue(it)
        }

        try {
            server.start()
            server.block(createNewClient())
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
