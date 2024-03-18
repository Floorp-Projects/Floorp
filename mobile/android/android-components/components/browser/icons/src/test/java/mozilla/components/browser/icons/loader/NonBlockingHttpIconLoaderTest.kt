/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.loader

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.icons.Icon
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
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Rule
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

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class NonBlockingHttpIconLoaderTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    @Test
    fun `Loader will return IconLoader#Result#NoResult for a load request and respond with the result through a callback`() = runTestOnMain {
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
                var callbackIconRequest: IconRequest? = null
                var callbackResource: IconRequest.Resource? = null
                var callbackIcon: IconLoader.Result? = null
                val loader = NonBlockingHttpIconLoader(client, scope) { request, resource, icon ->
                    callbackIconRequest = request
                    callbackResource = resource
                    callbackIcon = icon
                }
                val iconRequest: IconRequest = mock()

                val result = loader.load(
                    mock(),
                    iconRequest,
                    IconRequest.Resource(
                        url = server.url("/some/path").toString(),
                        type = IconRequest.Resource.Type.APPLE_TOUCH_ICON,
                    ),
                )

                assertTrue(result is IconLoader.Result.NoResult)
                val downloadedResource = String(((callbackIcon as IconLoader.Result.BytesResult).bytes), Charsets.UTF_8)
                assertEquals("Hello World!", downloadedResource)
                assertSame(Icon.Source.DOWNLOAD, ((callbackIcon as IconLoader.Result.BytesResult).source))
                assertTrue(callbackResource!!.url.endsWith("/some/path"))
                assertSame(IconRequest.Resource.Type.APPLE_TOUCH_ICON, callbackResource?.type)
                assertSame(iconRequest, callbackIconRequest)
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `Loader will not perform any requests for data uris`() = runTestOnMain {
        val client: Client = mock()
        var callbackIconRequest: IconRequest? = null
        var callbackResource: IconRequest.Resource? = null
        var callbackIcon: IconLoader.Result? = null
        val loader = NonBlockingHttpIconLoader(client, scope) { request, resource, icon ->
            callbackIconRequest = request
            callbackResource = resource
            callbackIcon = icon
        }

        val result = loader.load(
            mock(),
            mock(),
            IconRequest.Resource(
                url = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAA" +
                    "AAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==",
                type = IconRequest.Resource.Type.FAVICON,
            ),
        )

        assertEquals(IconLoader.Result.NoResult, result)
        assertNull(callbackIconRequest)
        assertNull(callbackResource)
        assertNull(callbackIcon)
        verify(client, never()).fetch(any())
    }

    @Test
    fun `Request has timeouts applied`() = runTestOnMain {
        val client: Client = mock()
        val loader = NonBlockingHttpIconLoader(client, scope) { _, _, _ -> }
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
    fun `NoResult is returned for non-successful requests`() = runTestOnMain {
        val client: Client = mock()
        var callbackIconRequest: IconRequest? = null
        var callbackResource: IconRequest.Resource? = null
        var callbackIcon: IconLoader.Result? = null
        val loader = NonBlockingHttpIconLoader(client, scope) { request, resource, icon ->
            callbackIconRequest = request
            callbackResource = resource
            callbackIcon = icon
        }
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
        assertEquals(IconLoader.Result.NoResult, callbackIcon)
        assertNotNull(callbackIconRequest)
        assertEquals("https://www.example.org", callbackResource!!.url)
        assertSame(IconRequest.Resource.Type.APPLE_TOUCH_ICON, callbackResource?.type)
    }

    @Test
    fun `Loader will not try to load URL again that just recently failed`() = runTestOnMain {
        val client: Client = mock()
        val loader = NonBlockingHttpIconLoader(client, scope) { _, _, _ -> }
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

        val result = loader.load(mock(), mock(), resource)

        assertEquals(IconLoader.Result.NoResult, result)
        // First load tries to fetch, but load fails (404)
        verify(client).fetch(any())
        verifyNoMoreInteractions(client)
        reset(client)
        assertEquals(IconLoader.Result.NoResult, loader.load(mock(), mock(), resource))
        // Second load does not try to fetch again.
        verify(client, never()).fetch(any())
    }

    @Test
    fun `Loader will return NoResult for IOExceptions happening during fetch`() = runTestOnMain {
        val client: Client = mock()
        doThrow(IOException("Mock")).`when`(client).fetch(any())
        var callbackIconRequest: IconRequest? = null
        var callbackResource: IconRequest.Resource? = null
        var callbackIcon: IconLoader.Result? = null
        val loader = NonBlockingHttpIconLoader(client, scope) { request, resource, icon ->
            callbackIconRequest = request
            callbackResource = resource
            callbackIcon = icon
        }

        val resource = IconRequest.Resource(
            url = "https://www.example.org",
            type = IconRequest.Resource.Type.APPLE_TOUCH_ICON,
        )

        val result = loader.load(testContext, mock(), resource)
        assertEquals(IconLoader.Result.NoResult, result)
        assertEquals(IconLoader.Result.NoResult, callbackIcon)
        assertNotNull(callbackIconRequest)
        assertEquals("https://www.example.org", callbackResource!!.url)
        assertSame(IconRequest.Resource.Type.APPLE_TOUCH_ICON, callbackResource?.type)
    }

    @Test
    fun `Loader will return NoResult for IOExceptions happening during toIconLoaderResult`() = runTestOnMain {
        val client: Client = mock()
        var callbackIconRequest: IconRequest? = null
        var callbackResource: IconRequest.Resource? = null
        var callbackIcon: IconLoader.Result? = null
        val loader = NonBlockingHttpIconLoader(client, scope) { request, resource, icon ->
            callbackIconRequest = request
            callbackResource = resource
            callbackIcon = icon
        }
        val failingStream: InputStream = object : InputStream() {
            override fun read(): Int {
                throw IOException("Kaboom")
            }
        }
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

        val result = loader.load(testContext, mock(), resource)

        assertEquals(IconLoader.Result.NoResult, result)
        assertEquals(IconLoader.Result.NoResult, callbackIcon)
        assertNotNull(callbackIconRequest)
        assertEquals("https://www.example.org", callbackResource!!.url)
        assertSame(IconRequest.Resource.Type.APPLE_TOUCH_ICON, callbackResource?.type)
    }

    @Test
    fun `Loader will sanitize URL`() = runTestOnMain {
        val client: Client = mock()
        val captor = argumentCaptor<Request>()
        val loader = NonBlockingHttpIconLoader(client, scope) { _, _, _ -> }
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

        verify(client).fetch(captor.capture())
        val request = captor.value
        assertEquals("https://www.example.org", request.url)
    }
}
