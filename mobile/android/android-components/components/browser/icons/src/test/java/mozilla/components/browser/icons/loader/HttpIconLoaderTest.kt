/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.loader

import mozilla.components.browser.icons.IconRequest
import mozilla.components.concept.fetch.Client
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.lib.fetch.okhttp.OkHttpClient
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class HttpIconLoaderTest {
    @Test
    fun `Loader downloads data and uses appropriate headers`() {
        val clients = listOf(
            HttpURLConnectionClient(),
            OkHttpClient()
        )

        clients.forEach { client ->
            val server = MockWebServer()

            server.enqueue(MockResponse().setBody(
                javaClass.getResourceAsStream("/misc/test.txt")!!
                    .bufferedReader()
                    .use { it.readText() }
            ))

            server.start()

            try {
                val loader = HttpIconLoader(client)
                val data = loader.load(
                    mock(), IconRequest.Resource(
                        url = server.url("/some/path").toString(),
                        type = IconRequest.Resource.Type.APPLE_TOUCH_ICON
                    )
                )

                assertNotNull(data!!)
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

        val data = HttpIconLoader(client).load(mock(), IconRequest.Resource(
            url = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAA" +
                "AAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==",
            type = IconRequest.Resource.Type.FAVICON
        ))

        assertNull(data)
        verify(client, never()).fetch(any())
    }
}
