/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.location

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.doThrow
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import java.io.IOException

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class MozillaLocationServiceTest {
    @Before
    @After
    fun cleanUp() {
        testContext.clearRegionCache()
    }

    @Test
    fun `WHEN calling fetchRegion AND the service returns a region THEN a Region object is returned`() = runTest {
        val server = MockWebServer()
        server.enqueue(MockResponse().setBody("{\"country_name\": \"Germany\", \"country_code\": \"DE\"}"))

        try {
            server.start()

            val service = MozillaLocationService(
                testContext,
                HttpURLConnectionClient(),
                apiKey = "test",
                serviceUrl = server.url("/").toString(),
            )

            val region = service.fetchRegion()

            assertNotNull(region!!)

            assertEquals("DE", region.countryCode)
            assertEquals("Germany", region.countryName)

            val request = server.takeRequest()

            assertEquals(server.url("/country?key=test"), request.requestUrl)
        } finally {
            server.shutdown()
        }
    }

    @Test
    fun `WHEN client throws IOException THEN the returned region is null`() = runTest {
        val client: Client = mock()
        doThrow(IOException()).`when`(client).fetch(any())

        val service = MozillaLocationService(testContext, client, apiKey = "test")
        val region = service.fetchRegion()

        assertNull(region)
    }

    @Test
    fun `WHEN fetching region THEN request is sent to the location service`() = runTest {
        val client: Client = mock()
        val response = Response(
            url = "http://example.org",
            status = 200,
            headers = MutableHeaders(),
            body = Response.Body("{\"country_name\": \"France\", \"country_code\": \"FR\"}".byteInputStream()),
        )
        doReturn(response).`when`(client).fetch(any())

        val service = MozillaLocationService(testContext, client, apiKey = "test")
        val region = service.fetchRegion()

        assertNotNull(region!!)

        assertEquals("FR", region.countryCode)
        assertEquals("France", region.countryName)

        val captor = argumentCaptor<Request>()
        verify(client).fetch(captor.capture())

        val request = captor.value
        assertEquals("https://location.services.mozilla.com/v1/country?key=test", request.url)
    }

    @Test
    fun `WHEN fetching region AND service returns 404 THEN region is null`() = runTest {
        val client: Client = mock()
        val response = Response(
            url = "http://example.org",
            status = 404,
            headers = MutableHeaders(),
            body = Response.Body.empty(),
        )
        doReturn(response).`when`(client).fetch(any())

        val service = MozillaLocationService(testContext, client, apiKey = "test")
        val region = service.fetchRegion()

        assertNull(region)
    }

    @Test
    fun `WHEN fetching region AND service returns 500 THEN region is null`() = runTest {
        val client: Client = mock()
        val response = Response(
            url = "http://example.org",
            status = 500,
            headers = MutableHeaders(),
            body = Response.Body("Internal Server Error".byteInputStream()),
        )
        doReturn(response).`when`(client).fetch(any())

        val service = MozillaLocationService(testContext, client, apiKey = "test")
        val region = service.fetchRegion()

        assertNull(region)
    }

    @Test
    fun `WHEN fetching region AND service returns broken JSON THEN region is null`() = runTest {
        val client: Client = mock()
        val response = Response(
            url = "http://example.org",
            status = 200,
            headers = MutableHeaders(),
            body = Response.Body("{\"country_name\": \"France\",".byteInputStream()),
        )
        doReturn(response).`when`(client).fetch(any())

        val service = MozillaLocationService(testContext, client, apiKey = "test")
        val region = service.fetchRegion()

        assertNull(region)
    }

    @Test
    fun `WHEN fetching region AND service returns empty JSON object THEN region is null`() = runTest {
        val client: Client = mock()
        val response = Response(
            url = "http://example.org",
            status = 200,
            headers = MutableHeaders(),
            body = Response.Body("{}".byteInputStream()),
        )
        doReturn(response).`when`(client).fetch(any())

        val service = MozillaLocationService(testContext, client, apiKey = "test")
        val region = service.fetchRegion()

        assertNull(region)
    }

    @Test
    fun `WHEN fetching region AND service returns incomplete JSON THEN region is null`() = runTest {
        val client: Client = mock()
        val response = Response(
            url = "http://example.org",
            status = 200,
            headers = MutableHeaders(),
            body = Response.Body("{\"country_code\": \"DE\"}".byteInputStream()),
        )
        doReturn(response).`when`(client).fetch(any())

        val service = MozillaLocationService(testContext, client, apiKey = "test")
        val region = service.fetchRegion()

        assertNull(region)
    }

    @Test
    fun `WHEN fetching region for the second time THEN region is read from cache`() = runTest {
        run {
            val client: Client = mock()
            val response = Response(
                url = "http://example.org",
                status = 200,
                headers = MutableHeaders(),
                body = Response.Body("{\"country_name\": \"Nepal\", \"country_code\": \"NP\"}".byteInputStream()),
            )
            doReturn(response).`when`(client).fetch(any())

            val service = MozillaLocationService(testContext, client, apiKey = "test")
            val region = service.fetchRegion()

            assertNotNull(region!!)

            assertEquals("NP", region.countryCode)
            assertEquals("Nepal", region.countryName)

            verify(client).fetch(any())
        }

        run {
            val client: Client = mock()

            val service = MozillaLocationService(testContext, client, apiKey = "test")
            val region = service.fetchRegion()

            assertNotNull(region!!)

            assertEquals("NP", region.countryCode)
            assertEquals("Nepal", region.countryName)

            verify(client, never()).fetch(any())
        }
    }

    @Test
    fun `WHEN fetching region for the second time and setting readFromCache = false THEN request is sent again`() = runTest {
        run {
            val client: Client = mock()
            val response = Response(
                url = "http://example.org",
                status = 200,
                headers = MutableHeaders(),
                body = Response.Body("{\"country_name\": \"Nepal\", \"country_code\": \"NP\"}".byteInputStream()),
            )
            doReturn(response).`when`(client).fetch(any())

            val service = MozillaLocationService(testContext, client, apiKey = "test")
            val region = service.fetchRegion()

            assertNotNull(region!!)

            assertEquals("NP", region.countryCode)
            assertEquals("Nepal", region.countryName)

            verify(client).fetch(any())
        }

        run {
            val client: Client = mock()
            val response = Response(
                url = "http://example.org",
                status = 200,
                headers = MutableHeaders(),
                body = Response.Body("{\"country_name\": \"Liberia\", \"country_code\": \"LR\"}".byteInputStream()),
            )
            doReturn(response).`when`(client).fetch(any())

            val service = MozillaLocationService(testContext, client, apiKey = "test")
            val region = service.fetchRegion(readFromCache = false)

            assertNotNull(region!!)

            assertEquals("LR", region.countryCode)
            assertEquals("Liberia", region.countryName)

            verify(client).fetch(any())
        }
    }
}
