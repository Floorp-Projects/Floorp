/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.fetch

import mozilla.components.support.test.expectException
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.lang.IllegalArgumentException

class HeadersTest {
    @Test
    fun `Creating Headers using constructor`() {
        val headers = MutableHeaders(
            "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Accept-Encoding" to "gzip, deflate",
            "Accept-Language" to "en-US,en;q=0.5",
            "Connection" to "keep-alive",
            "Dnt" to "1",
            "User-Agent" to "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.14; rv:65.0) Gecko/20100101 Firefox/65.0",
        )

        assertEquals(6, headers.size)

        assertEquals("Accept", headers[0].name)
        assertEquals("Accept-Encoding", headers[1].name)
        assertEquals("Accept-Language", headers[2].name)
        assertEquals("Connection", headers[3].name)
        assertEquals("Dnt", headers[4].name)
        assertEquals("User-Agent", headers[5].name)

        assertEquals("text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8", headers[0].value)
        assertEquals("gzip, deflate", headers[1].value)
        assertEquals("en-US,en;q=0.5", headers[2].value)
        assertEquals("keep-alive", headers[3].value)
        assertEquals("1", headers[4].value)
        assertEquals("Mozilla/5.0 (Macintosh; Intel Mac OS X 10.14; rv:65.0) Gecko/20100101 Firefox/65.0", headers[5].value)
    }

    @Test
    fun `Setting headers`() {
        val headers = MutableHeaders()

        headers.set("Accept-Encoding", "gzip, deflate")
        headers.set("Connection", "keep-alive")
        headers.set("Accept-Encoding", "gzip")
        headers.set("Dnt", "1")

        assertEquals(3, headers.size)

        assertEquals("Accept-Encoding", headers[0].name)
        assertEquals("Connection", headers[1].name)
        assertEquals("Dnt", headers[2].name)

        assertEquals("gzip", headers[0].value)
        assertEquals("keep-alive", headers[1].value)
        assertEquals("1", headers[2].value)
    }

    @Test
    fun `Appending headers`() {
        val headers = MutableHeaders()

        headers.append("Accept-Encoding", "gzip, deflate")
        headers.append("Connection", "keep-alive")
        headers.append("Accept-Encoding", "gzip")
        headers.append("Dnt", "1")

        assertEquals(4, headers.size)

        assertEquals("Accept-Encoding", headers[0].name)
        assertEquals("Connection", headers[1].name)
        assertEquals("Accept-Encoding", headers[2].name)
        assertEquals("Dnt", headers[3].name)

        assertEquals("gzip, deflate", headers[0].value)
        assertEquals("keep-alive", headers[1].value)
        assertEquals("gzip", headers[2].value)
        assertEquals("1", headers[3].value)
    }

    @Test
    fun `Overriding headers at index`() {
        val headers = MutableHeaders().apply {
            set("User-Agent", "Mozilla/5.0")
            set("Connection", "keep-alive")
            set("Accept-Encoding", "gzip")
        }

        headers[2] = Header("Dnt", "0")
        headers[0] = Header("Accept-Language", "en-US,en;q=0.5")

        assertEquals(3, headers.size)

        assertEquals("Accept-Language", headers[0].name)
        assertEquals("Connection", headers[1].name)
        assertEquals("Dnt", headers[2].name)

        assertEquals("en-US,en;q=0.5", headers[0].value)
        assertEquals("keep-alive", headers[1].value)
        assertEquals("0", headers[2].value)
    }

    @Test
    fun `Contains header with name`() {
        val headers = MutableHeaders().apply {
            set("User-Agent", "Mozilla/5.0")
            set("Connection", "keep-alive")
            set("Accept-Encoding", "gzip")
        }

        assertTrue(headers.contains("User-Agent"))
        assertTrue(headers.contains("Connection"))
        assertTrue(headers.contains("Accept-Encoding"))

        assertFalse(headers.contains("Accept-Language"))
        assertFalse(headers.contains("Dnt"))
        assertFalse(headers.contains("Accept"))
    }

    @Test
    fun `Throws if header name is empty`() {
        expectException(IllegalArgumentException::class) {
            MutableHeaders(
                "" to "Mozilla/5.0",
            )
        }

        expectException(IllegalArgumentException::class) {
            MutableHeaders()
                .append("", "Mozilla/5.0")
        }

        expectException(IllegalArgumentException::class) {
            MutableHeaders()
                .set("", "Mozilla/5.0")
        }

        expectException(IllegalArgumentException::class) {
            Header("", "Mozilla/5.0")
        }
    }

    @Test
    fun `Iterator usage`() {
        val headers = MutableHeaders().apply {
            set("User-Agent", "Mozilla/5.0")
            set("Connection", "keep-alive")
            set("Accept-Encoding", "gzip")
        }

        var i = 0
        headers.forEach { _ -> i++ }

        assertEquals(3, i)

        assertNotNull(headers.firstOrNull { header -> header.name == "User-Agent" })
    }

    @Test
    fun `Creating and modifying headers`() {
        val headers = MutableHeaders(
            "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Accept-Encoding" to "gzip, deflate",
            "Accept-Language" to "en-US,en;q=0.5",
            "Connection" to "keep-alive",
            "Dnt" to "1",
            "User-Agent" to "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.14; rv:65.0) Gecko/20100101 Firefox/65.0",
        )

        headers.set("Dnt", "0")
        headers.set("User-Agent", "Mozilla/6.0")
        headers.append("Accept", "*/*")

        assertEquals(7, headers.size)

        assertEquals("Accept", headers[0].name)
        assertEquals("Accept-Encoding", headers[1].name)
        assertEquals("Accept-Language", headers[2].name)
        assertEquals("Connection", headers[3].name)
        assertEquals("Dnt", headers[4].name)
        assertEquals("User-Agent", headers[5].name)
        assertEquals("Accept", headers[6].name)

        assertEquals("text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8", headers[0].value)
        assertEquals("gzip, deflate", headers[1].value)
        assertEquals("en-US,en;q=0.5", headers[2].value)
        assertEquals("keep-alive", headers[3].value)
        assertEquals("0", headers[4].value)
        assertEquals("Mozilla/6.0", headers[5].value)
        assertEquals("*/*", headers[6].value)
    }

    @Test
    fun `In operator`() {
        val headers = MutableHeaders().apply {
            set("User-Agent", "Mozilla/5.0")
            set("Connection", "keep-alive")
            set("Accept-Encoding", "gzip")
        }

        assertTrue("User-Agent" in headers)
        assertTrue("Connection" in headers)
        assertTrue("Accept-Encoding" in headers)

        assertFalse("Accept-Language" in headers)
        assertFalse("Accept" in headers)
        assertFalse("Dnt" in headers)
    }

    @Test
    fun `Get multiple headers by name`() {
        val headers = MutableHeaders().apply {
            append("Accept-Encoding", "gzip")
            append("Accept-Encoding", "deflate")
            append("Connection", "keep-alive")
        }

        val values = headers.getAll("Accept-Encoding")
        assertEquals(2, values.size)
        assertEquals("gzip", values[0])
        assertEquals("deflate", values[1])
    }

    @Test
    fun `Getting headers by name`() {
        val headers = MutableHeaders().apply {
            append("Accept-Encoding", "gzip")
            append("Accept-Encoding", "deflate")
            append("Connection", "keep-alive")
        }

        assertEquals("deflate", headers["Accept-Encoding"])
        assertEquals("keep-alive", headers["Connection"])
    }
}
