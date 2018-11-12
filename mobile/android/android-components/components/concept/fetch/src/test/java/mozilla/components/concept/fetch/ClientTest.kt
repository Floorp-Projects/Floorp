/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.fetch

import kotlinx.coroutines.async
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ClientTest {
    @Test
    fun `Verify default user agent format`() {
        val client = TestClient()

        val userAgent = client.exposeDefaultHeaders()["User-Agent"]
        assertNotNull(userAgent!!)

        assertTrue(userAgent.matches("MozacFetch/[0-9]+\\.[0-9]+\\.[0-9]+".toRegex()))
    }

    @Test
    fun `Expects gzip encoding default header`() {
        val client = TestClient()

        val acceptEncoding = client.exposeDefaultHeaders().get("Accept-Encoding")
        assertNotNull(acceptEncoding!!)
        assertEquals("gzip", acceptEncoding)
    }

    @Test
    fun `Expects accept all default header`() {
        val client = TestClient()

        val accept = client.exposeDefaultHeaders().get("Accept")
        assertNotNull(accept!!)
        assertEquals("*/*", accept)
    }

    @Test
    fun `Expects keep-alive connection default header`() {
        val client = TestClient()

        val connection = client.exposeDefaultHeaders().get("Connection")
        assertNotNull(connection!!)
        assertEquals("keep-alive", connection)
    }

    @Test
    fun `Async request with coroutines`() = runBlocking {
        val client = TestClient(responseBody = Response.Body("Hello World".byteInputStream()))
        val request = Request("https://www.mozilla.org")

        val deferredResponse = async { client.fetch(request) }

        val body = deferredResponse.await().body.string()
        assertEquals("Hello World", body)
    }
}

private class TestClient(
    private val responseUrl: String? = null,
    private val responseStatus: Int = 200,
    private val responseHeaders: Headers = MutableHeaders(),
    private val responseBody: Response.Body = Response.Body.empty()
) : Client() {
    override fun fetch(request: Request): Response {
        return Response(responseUrl ?: request.url, responseStatus, responseHeaders, responseBody)
    }

    fun exposeDefaultHeaders() = defaultHeaders
}
