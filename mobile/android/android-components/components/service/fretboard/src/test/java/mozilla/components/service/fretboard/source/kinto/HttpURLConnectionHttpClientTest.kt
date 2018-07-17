/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.source.kinto

import mozilla.components.service.fretboard.ExperimentDownloadException
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okhttp3.mockwebserver.RecordedRequest
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import java.net.URL

@RunWith(RobolectricTestRunner::class)
class HttpURLConnectionHttpClientTest {
    @Test(expected = ExperimentDownloadException::class)
    fun testGETIOException() {
        val server = MockWebServer()
        try {
            val serverUrl = server.url("/").url().toString()
            HttpURLConnectionHttpClient().get(URL(serverUrl.replace(server.port.toString(), (server.port + 1).toString())))
        } finally {
            server.shutdown()
        }
    }

    @Test(expected = ExperimentDownloadException::class)
    fun testGET404() {
        testGETError(404)
    }

    @Test(expected = ExperimentDownloadException::class)
    fun testGET500() {
        testGETError(500)
    }

    @Test
    fun testGETWithoutHeaders() {
        testGET()
    }

    @Test
    fun testGETWithHeaders() {
        testGET(mapOf("Accept" to "application/json")) {
            assertEquals("application/json", it.headers["Accept"])
        }
    }

    private fun testGET(headers: Map<String, String>? = null, assertions: ((RecordedRequest) -> Unit)? = null) {
        val response = """{"data":[]}"""
        val server = MockWebServer()
        server.enqueue(MockResponse().setBody(response))
        assertEquals(response, HttpURLConnectionHttpClient().get(server.url("/").url(), headers))
        val request = server.takeRequest()
        assertEquals("GET", request.method)
        assertEquals("no-cache", request.headers["Cache-Control"])
        assertEquals("no-cache", request.headers["Pragma"])
        if (assertions != null) {
            assertions(request)
        }
        server.shutdown()
    }

    private fun testGETError(responseCode: Int) {
        val server = MockWebServer()
        server.enqueue(MockResponse().setResponseCode(responseCode))
        try {
            HttpURLConnectionHttpClient().get(server.url("/").url())
        } finally {
            server.shutdown()
        }
    }
}