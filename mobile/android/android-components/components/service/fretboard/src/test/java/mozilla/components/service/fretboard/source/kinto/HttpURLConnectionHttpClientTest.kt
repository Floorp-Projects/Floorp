/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.source.kinto

import mozilla.components.service.fretboard.ExperimentDownloadException
import mozilla.components.support.test.mock
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okhttp3.mockwebserver.RecordedRequest
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.doThrow
import org.mockito.Mockito.mock
import org.robolectric.RobolectricTestRunner
import java.io.IOException
import java.net.HttpURLConnection
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
    fun testNonHTTPURL() {
        HttpURLConnectionHttpClient().get(URL("file://filename.json"))
    }

    @Test(expected = ExperimentDownloadException::class)
    fun testOpenConnectionException() {
        val url = mock(URL::class.java)
        `when`(url.openConnection()).thenThrow(IOException())
        HttpURLConnectionHttpClient().get(url)
    }

    @Test(expected = ExperimentDownloadException::class)
    fun testGET404() {
        testGETResponseCode(404)
    }

    @Test(expected = ExperimentDownloadException::class)
    fun testGET500() {
        testGETResponseCode(500)
    }

    @Test(expected = ExperimentDownloadException::class)
    fun testGET199() {
        testGETResponseCode(199)
    }

    @Test(expected = ExperimentDownloadException::class)
    fun testGET300() {
        testGETResponseCode(300)
    }

    @Test
    fun testGET299() {
        testGET(responseCode = 299)
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

    /**
     * On some devices we are seeing an ArrayIndexOutOfBoundsException somewhere inside AOSP/okhttp.
     *
     * See:
     * https://github.com/mozilla-mobile/android-components/issues/964
     */
    @Test(expected = ExperimentDownloadException::class)
    fun testArrayIndexOutOfBoundsException() {
        val client = HttpURLConnectionHttpClient()

        val connection: HttpURLConnection = mock()
        val url: URL = mock()
        doReturn(connection).`when`(url).openConnection()
        doThrow(ArrayIndexOutOfBoundsException()).`when`(connection).responseCode

        client.get(url, mapOf()) // Should wrap exception and throw ExperimentDownloadException
    }

    private fun testGET(headers: Map<String, String>? = null, responseCode: Int = 200, assertions: ((RecordedRequest) -> Unit)? = null) {
        val response = """{"data":[]}"""
        val server = MockWebServer()
        server.enqueue(MockResponse().setResponseCode(responseCode).setBody(response))
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

    private fun testGETResponseCode(responseCode: Int) {
        val server = MockWebServer()
        server.enqueue(MockResponse().setResponseCode(responseCode))
        try {
            HttpURLConnectionHttpClient().get(server.url("/").url())
        } finally {
            server.shutdown()
        }
    }
}