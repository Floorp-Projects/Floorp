/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers.Names.USER_AGENT
import mozilla.components.concept.fetch.Response
import mozilla.components.service.pocket.helpers.MockResponses
import mozilla.components.service.pocket.helpers.assertRequestParams
import mozilla.components.service.pocket.helpers.assertResponseIsClosed
import mozilla.components.service.pocket.helpers.assertSuccessfulRequestReturnsResponseBody
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import java.io.IOException

private const val VALID_API_KEY = "apiKey"
private const val VALID_USER_AGENT = "userAgent"

private val TEST_URL = "https://mozilla.org".toUri()

// From Firefox for Fire TV
private const val TEST_USER_AGENT = "Mozilla/5.0 (Linux; Android 7.1.2) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Focus/3.0 Chrome/59.0.3017.125 Safari/537.36"

@RunWith(AndroidJUnit4::class)
class PocketEndpointRawTest {

    private lateinit var endpoint: PocketEndpointRaw
    private lateinit var client: Client
    private lateinit var urls: PocketURLs

    private lateinit var errorResponse: Response
    private lateinit var successResponse: Response
    private lateinit var defaultResponse: Response

    @Before
    fun setUp() {
        errorResponse = MockResponses.getError()
        successResponse = MockResponses.getSuccess()
        defaultResponse = errorResponse

        client = mock<Client>().also {
            `when`(it.fetch(any())).thenReturn(defaultResponse)
        }

        urls = mock<PocketURLs>().also {
            `when`(it.globalVideoRecs).thenReturn(TEST_URL)
        }
        endpoint = PocketEndpointRaw(client, urls, TEST_USER_AGENT)
    }

    @Test
    fun `WHEN requesting global video recs THEN the global video recs url is used`() {
        val expectedUrl = "https://mozilla.org/global-video-recs"
        `when`(urls.globalVideoRecs).thenReturn(expectedUrl.toUri())

        assertRequestParams(client, makeRequest = {
            endpoint.getGlobalVideoRecommendations()
        }, assertParams = { request ->
            assertEquals(expectedUrl, request.url)
        })
    }

    @Test
    fun `WHEN requesting global video recs THEN the headers include the user agent`() {
        assertRequestParams(client, makeRequest = {
            endpoint.getGlobalVideoRecommendations()
        }, assertParams = { request ->
            val userAgent = request.headers?.get(USER_AGENT)
            assertEquals(TEST_USER_AGENT, userAgent)
        })
    }

    @Test
    fun `WHEN requesting global video recs and the client throws an IOException THEN null is returned`() {
        `when`(client.fetch(any())).thenThrow(IOException::class.java)
        assertNull(endpoint.getGlobalVideoRecommendations())
    }

    @Test
    fun `WHEN requesting global video recs and the response is null THEN null is returned`() {
        `when`(client.fetch(any())).thenReturn(null)
        assertNull(endpoint.getGlobalVideoRecommendations())
    }

    @Test
    fun `WHEN requesting global video recs and the response is not a success THEN null is returned`() {
        `when`(client.fetch(any())).thenReturn(errorResponse)
        assertNull(endpoint.getGlobalVideoRecommendations())
    }

    @Test
    fun `WHEN requesting global video recs and the response is a success THEN the response body is returned`() {
        assertSuccessfulRequestReturnsResponseBody(client, endpoint::getGlobalVideoRecommendations)
    }

    @Test
    fun `WHEN requesting global video recs and the response is an error THEN response is closed`() {
        assertResponseIsClosed(client, errorResponse) {
            endpoint.getGlobalVideoRecommendations()
        }
    }

    @Test
    fun `WHEN requesting global video recs and the response is a success THEN response is closed`() {
        assertResponseIsClosed(client, successResponse) {
            endpoint.getGlobalVideoRecommendations()
        }
    }

    @Test(expected = IllegalArgumentException::class)
    fun `WHEN newInstance is called with a blank API key THEN an exception is thrown`() {
        PocketEndpointRaw.newInstance(client, " ", VALID_USER_AGENT)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `WHEN newInstance is called with a blank user agent THEN an exception is thrown`() {
        PocketEndpoint.newInstance(client, VALID_API_KEY, " ")
    }

    @Test
    fun `WHEN newInstance is called with valid args THEN no exception is thrown`() {
        PocketEndpoint.newInstance(client, VALID_API_KEY, VALID_USER_AGENT)
    }
}
