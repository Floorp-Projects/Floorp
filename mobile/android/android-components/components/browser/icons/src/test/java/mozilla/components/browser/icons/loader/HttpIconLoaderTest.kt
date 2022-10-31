/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.loader

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.icons.IconRequest
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.lib.fetch.okhttp.OkHttpClient
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.doThrow
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import java.io.IOException
import java.io.InputStream

@RunWith(AndroidJUnit4::class)
class HttpIconLoaderTest {

    @Test
    fun `Loader downloads data and uses appropriate headers`() {
        val clients = listOf(
            HttpURLConnectionClient(),
            OkHttpClient(),
        )

        clients.forEach { client ->
            val server = MockWebServer()

            server.enqueue(
                MockResponse().setBody(
                    javaClass.getResourceAsStream("/misc/test.txt")!!
                        .bufferedReader()
                        .use { it.readText() },
                ),
            )

            server.start()

            try {
                val loader = HttpIconLoader(client)
                val result = loader.load(
                    mock(),
                    mock(),
                    IconRequest.Resource(
                        url = server.url("/some/path").toString(),
                        type = IconRequest.Resource.Type.APPLE_TOUCH_ICON,
                    ),
                )

                assertTrue(result is IconLoader.Result.BytesResult)

                val data = (result as IconLoader.Result.BytesResult).bytes

                assertTrue(data.isNotEmpty())

                val text = String(data, Charsets.UTF_8)

                assertEquals("Hello World!", text)

                val request = server.takeRequest()

                assertEquals("GET", request.method)

                val headers = request.headers
                for (i in 0 until headers.size()) {
                    println(headers.name(i) + ": " + headers.value(i))
                }
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `Loader will not perform any requests for data uris`() {
        val client: Client = mock()

        val result = HttpIconLoader(client).load(
            mock(),
            mock(),
            IconRequest.Resource(
                url = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAA" +
                    "AAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==",
                type = IconRequest.Resource.Type.FAVICON,
            ),
        )

        assertEquals(IconLoader.Result.NoResult, result)
        verify(client, never()).fetch(any())
    }

    @Test
    fun `Request has timeouts applied`() {
        val client: Client = mock()

        val loader = HttpIconLoader(client)
        doReturn(
            Response(
                url = "https://www.example.org",
                headers = MutableHeaders(),
                status = 404,
                body = Response.Body.empty(),
            ),
        ).`when`(client).fetch(any())

        loader.load(
            mock(),
            mock(),
            IconRequest.Resource(
                url = "https://www.example.org",
                type = IconRequest.Resource.Type.APPLE_TOUCH_ICON,
            ),
        )

        val captor = argumentCaptor<Request>()
        verify(client).fetch(captor.capture())

        val request = captor.value
        assertNotNull(request)
        assertNotNull(request.connectTimeout)
        assertNotNull(request.readTimeout)
    }

    @Test
    fun `NoResult is returned for non-successful requests`() {
        val client: Client = mock()

        val loader = HttpIconLoader(client)
        doReturn(
            Response(
                url = "https://www.example.org",
                headers = MutableHeaders(),
                status = 404,
                body = Response.Body.empty(),
            ),
        ).`when`(client).fetch(any())

        val result = loader.load(
            mock(),
            mock(),
            IconRequest.Resource(
                url = "https://www.example.org",
                type = IconRequest.Resource.Type.APPLE_TOUCH_ICON,
            ),
        )

        assertEquals(IconLoader.Result.NoResult, result)
    }

    @Test
    fun `Loader will not try to load URL again that just recently failed`() {
        val client: Client = mock()

        val loader = HttpIconLoader(client)
        doReturn(
            Response(
                url = "https://www.example.org",
                headers = MutableHeaders(),
                status = 404,
                body = Response.Body.empty(),
            ),
        ).`when`(client).fetch(any())

        val resource = IconRequest.Resource(
            url = "https://www.example.org",
            type = IconRequest.Resource.Type.APPLE_TOUCH_ICON,
        )

        assertEquals(IconLoader.Result.NoResult, loader.load(mock(), mock(), resource))

        // First load tries to fetch, but load fails (404)
        verify(client).fetch(any())
        verifyNoMoreInteractions(client)
        reset(client)

        assertEquals(IconLoader.Result.NoResult, loader.load(mock(), mock(), resource))

        // Second load does not try to fetch again.
        verify(client, never()).fetch(any())
    }

    @Test
    fun `Loader will return NoResult for IOExceptions happening during fetch`() {
        val client: Client = mock()
        doThrow(IOException("Mock")).`when`(client).fetch(any())

        val loader = HttpIconLoader(client)

        val resource = IconRequest.Resource(
            url = "https://www.example.org",
            type = IconRequest.Resource.Type.APPLE_TOUCH_ICON,
        )

        assertEquals(IconLoader.Result.NoResult, loader.load(testContext, mock(), resource))
    }

    @Test
    fun `Loader will return NoResult for IOExceptions happening during toIconLoaderResult`() {
        val client: Client = mock()

        val failingStream: InputStream = object : InputStream() {
            override fun read(): Int {
                throw IOException("Kaboom")
            }
        }

        val loader = HttpIconLoader(client)
        doReturn(
            Response(
                url = "https://www.example.org",
                headers = MutableHeaders(),
                status = 200,
                body = Response.Body(failingStream),
            ),
        ).`when`(client).fetch(any())

        val resource = IconRequest.Resource(
            url = "https://www.example.org",
            type = IconRequest.Resource.Type.APPLE_TOUCH_ICON,
        )

        assertEquals(IconLoader.Result.NoResult, loader.load(mock(), mock(), resource))
    }

    @Test
    fun `Loader will sanitize URL`() {
        val client: Client = mock()

        val loader = HttpIconLoader(client)
        doReturn(
            Response(
                url = "https://www.example.org",
                headers = MutableHeaders(),
                status = 404,
                body = Response.Body.empty(),
            ),
        ).`when`(client).fetch(any())

        loader.load(
            mock(),
            mock(),
            IconRequest.Resource(
                url = " \n\n https://www.example.org  \n\n ",
                type = IconRequest.Resource.Type.APPLE_TOUCH_ICON,
            ),
        )

        val captor = argumentCaptor<Request>()
        verify(client).fetch(captor.capture())

        val request = captor.value
        assertEquals("https://www.example.org", request.url)
    }
}
