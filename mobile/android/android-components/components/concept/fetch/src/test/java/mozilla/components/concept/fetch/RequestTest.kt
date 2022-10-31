/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.fetch

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doThrow
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.net.URLEncoder
import java.util.UUID
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
class RequestTest {

    @Test
    fun `URL-only Request`() {
        val request = Request("https://www.mozilla.org")

        assertEquals("https://www.mozilla.org", request.url)
        assertEquals(Request.Method.GET, request.method)
    }

    @Test
    fun `Fully configured Request`() {
        val request = Request(
            url = "https://www.mozilla.org",
            method = Request.Method.POST,
            headers = MutableHeaders(
                "Accept-Language" to "en-US,en;q=0.5",
                "Connection" to "keep-alive",
                "Dnt" to "1",
            ),
            connectTimeout = Pair(10, TimeUnit.SECONDS),
            readTimeout = Pair(1, TimeUnit.MINUTES),
            body = Request.Body.fromString("Hello World!"),
            redirect = Request.Redirect.MANUAL,
            cookiePolicy = Request.CookiePolicy.INCLUDE,
            useCaches = true,
        )

        assertEquals("https://www.mozilla.org", request.url)
        assertEquals(Request.Method.POST, request.method)

        assertEquals(10, request.connectTimeout!!.first)
        assertEquals(TimeUnit.SECONDS, request.connectTimeout!!.second)

        assertEquals(1, request.readTimeout!!.first)
        assertEquals(TimeUnit.MINUTES, request.readTimeout!!.second)

        assertEquals("Hello World!", request.body!!.useStream { it.bufferedReader().readText() })
        assertEquals(Request.Redirect.MANUAL, request.redirect)
        assertEquals(Request.CookiePolicy.INCLUDE, request.cookiePolicy)
        assertEquals(true, request.useCaches)

        val headers = request.headers!!
        assertEquals(3, headers.size)

        assertEquals("Accept-Language", headers[0].name)
        assertEquals("Connection", headers[1].name)
        assertEquals("Dnt", headers[2].name)

        assertEquals("en-US,en;q=0.5", headers[0].value)
        assertEquals("keep-alive", headers[1].value)
        assertEquals("1", headers[2].value)
    }

    @Test
    fun `Create request body from string`() {
        val body = Request.Body.fromString("Hello World")
        assertEquals("Hello World", body.readText())
    }

    @Test
    fun `Create request body from file`() {
        val file = File.createTempFile(UUID.randomUUID().toString(), UUID.randomUUID().toString())
        file.writer().use { it.write("Banana") }

        val body = Request.Body.fromFile(file)
        assertEquals("Banana", body.readText())
    }

    @Test
    fun `WHEN creating a request body from empty params THEN the empty string is returned`() {
        assertEquals("", Request.Body.fromParamsForFormUrlEncoded().readText())
    }

    @Test
    fun `WHEN creating a request body from params with empty keys or values THEN they are represented as the empty string in the result`() {
        // In practice, we don't expect anyone to do this but this test is here as to documentation of what happens.
        val expected = "=value&hello=world&key="
        val body = Request.Body.fromParamsForFormUrlEncoded(
            "" to "value",
            "hello" to "world",
            "key" to "",
        )
        assertEquals(expected, body.readText())
    }

    @Test
    fun `WHEN creating a request body from non-alphabetized params for urlencoded THEN it's in the correct format and ordering`() {
        val inputUrl = "https://github.com/mozilla-mobile/android-components/issues/2394"
        val encodedURL = URLEncoder.encode(inputUrl, Charsets.UTF_8.name())
        val expected = "v=2&url=$encodedURL"

        val body = Request.Body.fromParamsForFormUrlEncoded(
            "v" to "2",
            "url" to inputUrl,
        )
        assertEquals(expected, body.readText())
    }

    @Test
    fun `Closing body closes stream`() {
        val stream: InputStream = mock()

        val body = Request.Body(stream)

        verify(stream, never()).close()

        body.close()

        verify(stream).close()
    }

    @Test
    fun `Using stream closes stream`() {
        val stream: InputStream = mock()

        val body = Request.Body(stream)

        verify(stream, never()).close()

        body.useStream {
            // Do nothing
        }

        verify(stream).close()
    }

    @Test
    fun `Stream throwing on close`() {
        val stream: InputStream = mock()
        doThrow(IOException()).`when`(stream).close()

        val body = Request.Body(stream)
        body.close()
    }

    @Test(expected = IllegalStateException::class)
    fun `useStream rethrows and closes stream`() {
        val stream: InputStream = mock()
        val body = Request.Body(stream)

        try {
            body.useStream {
                throw IllegalStateException()
            }
        } finally {
            verify(stream).close()
        }
    }

    @Test
    fun `Is a blob Request`() {
        var request = Request(url = "blob:https://mdn.mozillademos.org/d518464c-5075-9046")

        assertTrue(request.isBlobUri())

        request = Request(url = "https://mdn.mozillademos.org/d518464c-5075-9046")

        assertFalse(request.isBlobUri())
    }
}

private fun Request.Body.readText(): String = useStream { it.bufferedReader().readText() }
