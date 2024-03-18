/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.fetch

import mozilla.components.concept.fetch.Headers.Names.CONTENT_TYPE
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.io.IOException
import java.io.InputStream

class ResponseTest {
    @Test
    fun `Creating String from Body`() {
        val stream = "Hello World".byteInputStream()

        val body = spy(Response.Body(stream))
        assertEquals("Hello World", body.string())

        verify(body).close()
    }

    @Test
    fun `Creating BufferedReader from Body`() {
        val stream = "Hello World".byteInputStream()

        val body = spy(Response.Body(stream))

        var readerUsed = false
        body.useBufferedReader { reader ->
            assertEquals("Hello World", reader.readText())
            readerUsed = true
        }

        assertTrue(readerUsed)

        verify(body).close()
    }

    @Test
    fun `Creating BufferedReader from Body with custom Charset `() {
        var stream = "ÄäÖöÜü".byteInputStream(Charsets.ISO_8859_1)
        var body = spy(Response.Body(stream, "text/plain; charset=UTF-8"))
        var readerUsed = false
        body.useBufferedReader { reader ->
            assertNotEquals("ÄäÖöÜü", reader.readText())
            readerUsed = true
        }
        assertTrue(readerUsed)

        stream = "ÄäÖöÜü".byteInputStream(Charsets.ISO_8859_1)
        body = spy(Response.Body(stream, "text/plain; charset=UTF-8"))
        readerUsed = false
        body.useBufferedReader(Charsets.ISO_8859_1) { reader ->
            assertEquals("ÄäÖöÜü", reader.readText())
            readerUsed = true
        }
        assertTrue(readerUsed)

        verify(body).close()
    }

    @Test
    fun `Creating String from Body with custom Charset `() {
        var stream = "ÄäÖöÜü".byteInputStream(Charsets.ISO_8859_1)
        var body = spy(Response.Body(stream, "text/plain; charset=UTF-8"))
        assertNotEquals("ÄäÖöÜü", body.string())

        stream = "ÄäÖöÜü".byteInputStream(Charsets.ISO_8859_1)
        body = spy(Response.Body(stream, "text/plain; charset=UTF-8"))
        assertEquals("ÄäÖöÜü", body.string(Charsets.ISO_8859_1))

        verify(body).close()
    }

    @Test
    fun `Creating Body with invalid charset falls back to UTF-8`() {
        var stream = "ÄäÖöÜü".byteInputStream(Charsets.UTF_8)
        var body = spy(Response.Body(stream, "text/plain; charset=invalid"))
        var readerUsed = false
        body.useBufferedReader { reader ->
            assertEquals("ÄäÖöÜü", reader.readText())
            readerUsed = true
        }
        assertTrue(readerUsed)

        verify(body).close()
    }

    @Test
    fun `Using InputStream from Body`() {
        val body = spy(Response.Body("Hello World".byteInputStream()))

        var streamUsed = false
        body.useStream { stream ->
            assertEquals("Hello World", stream.bufferedReader().readText())
            streamUsed = true
        }

        assertTrue(streamUsed)

        verify(body).close()
    }

    @Test
    fun `Closing Body closes stream`() {
        val stream = spy("Hello World".byteInputStream())

        val body = spy(Response.Body(stream))
        body.close()

        verify(stream).close()
    }

    @Test
    fun `success() extension function returns true for 2xx response codes`() {
        assertTrue(Response("https://www.mozilla.org", 200, headers = mock(), body = mock()).isSuccess)
        assertTrue(Response("https://www.mozilla.org", 203, headers = mock(), body = mock()).isSuccess)

        assertFalse(Response("https://www.mozilla.org", 404, headers = mock(), body = mock()).isSuccess)
        assertFalse(Response("https://www.mozilla.org", 500, headers = mock(), body = mock()).isSuccess)
        assertFalse(Response("https://www.mozilla.org", 302, headers = mock(), body = mock()).isSuccess)
    }

    @Test
    fun `clientError() extension function returns true for 4xx response codes`() {
        assertTrue(Response("https://www.mozilla.org", 404, headers = mock(), body = mock()).isClientError)
        assertTrue(Response("https://www.mozilla.org", 403, headers = mock(), body = mock()).isClientError)

        assertFalse(Response("https://www.mozilla.org", 200, headers = mock(), body = mock()).isClientError)
        assertFalse(Response("https://www.mozilla.org", 203, headers = mock(), body = mock()).isClientError)
        assertFalse(Response("https://www.mozilla.org", 500, headers = mock(), body = mock()).isClientError)
        assertFalse(Response("https://www.mozilla.org", 302, headers = mock(), body = mock()).isClientError)
    }

    @Test
    fun `Fully configured Response`() {
        val response = Response(
            url = "https://www.mozilla.org",
            status = 200,
            headers = MutableHeaders(
                CONTENT_TYPE to "text/html; charset=utf-8",
                "Connection" to "Close",
                "Expires" to "Thu, 08 Nov 2018 15:41:43 GMT",
            ),
            body = Response.Body("Hello World".byteInputStream()),
        )

        assertEquals("https://www.mozilla.org", response.url)
        assertEquals(200, response.status)
        assertEquals("Hello World", response.body.string())

        val headers = response.headers
        assertEquals(3, headers.size)

        assertEquals("Content-Type", headers[0].name)
        assertEquals("Connection", headers[1].name)
        assertEquals("Expires", headers[2].name)

        assertEquals("text/html; charset=utf-8", headers[0].value)
        assertEquals("Close", headers[1].value)
        assertEquals("Thu, 08 Nov 2018 15:41:43 GMT", headers[2].value)
    }

    @Test
    fun `Closing body closes stream of body`() {
        val stream: InputStream = mock()
        val response = Response("url", 200, MutableHeaders(), Response.Body(stream))

        verify(stream, never()).close()

        response.body.close()

        verify(stream).close()
    }

    @Test
    fun `Closing response closes stream of body`() {
        val stream: InputStream = mock()
        val response = Response("url", 200, MutableHeaders(), Response.Body(stream))

        verify(stream, never()).close()

        response.close()

        verify(stream).close()
    }

    @Test
    fun `Empty body`() {
        val body = Response.Body.empty()
        assertEquals("", body.string())
    }

    @Test
    fun `Creating string closes stream`() {
        val stream: InputStream = spy("".byteInputStream())
        val body = Response.Body(stream)

        verify(stream, never()).close()

        body.string()

        verify(stream).close()
    }

    @Test(expected = TestException::class)
    fun `Using buffered reader closes stream`() {
        val stream: InputStream = spy("".byteInputStream())
        val body = Response.Body(stream)

        verify(stream, never()).close()

        try {
            body.useBufferedReader {
                throw TestException()
            }
        } finally {
            verify(stream).close()
        }
    }

    @Test(expected = TestException::class)
    fun `Using stream closes stream`() {
        val stream: InputStream = spy("".byteInputStream())
        val body = Response.Body(stream)

        verify(stream, never()).close()

        try {
            body.useStream {
                throw TestException()
            }
        } finally {
            verify(stream).close()
        }
    }

    @Test
    fun `Stream throwing on close`() {
        val stream: InputStream = mock()
        Mockito.doThrow(IOException()).`when`(stream).close()

        val body = Response.Body(stream)
        body.close()
    }
}

private class TestException : RuntimeException()
