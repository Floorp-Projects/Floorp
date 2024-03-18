/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.fetch

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.async
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Test

class ClientTest {
    @ExperimentalCoroutinesApi
    @Test
    fun `Async request with coroutines`() = runTest {
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
    private val responseBody: Response.Body = Response.Body.empty(),
) : Client() {
    override fun fetch(request: Request): Response {
        return Response(responseUrl ?: request.url, responseStatus, responseHeaders, responseBody)
    }
}
